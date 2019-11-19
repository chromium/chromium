// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/logo_service_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_provider_logos/features.h"
#include "components/search_provider_logos/fixed_logo_api.h"
#include "components/search_provider_logos/google_logo_api.h"
#include "components/search_provider_logos/logo_cache.h"
#include "components/search_provider_logos/logo_observer.h"
#include "components/search_provider_logos/switches.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/gfx/image/image.h"

namespace search_provider_logos {
namespace {

const int64_t kMaxDownloadBytes = 1024 * 1024;
const int kDecodeLogoTimeoutSeconds = 30;

// Implements a callback for image_fetcher::ImageDecoder. If Run() is called on
// a callback returned by GetCallback() within 30 seconds, forwards the decoded
// image to the wrapped callback. If not, sends an empty image to the wrapped
// callback instead. Either way, deletes the object and prevents further calls.
//
// TODO(sfiera): find a more idiomatic way of setting a deadline on the
// callback. This is implemented as a self-deleting object in part because it
// needed to when it used to be a delegate and in part because I couldn't figure
// out a better way, now that it isn't.
class ImageDecodedHandlerWithTimeout {
 public:
  static base::RepeatingCallback<void(const gfx::Image&)> Wrap(
      const base::RepeatingCallback<void(const SkBitmap&)>&
          image_decoded_callback) {
    auto* handler = new ImageDecodedHandlerWithTimeout(image_decoded_callback);
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ImageDecodedHandlerWithTimeout::OnImageDecoded,
                       handler->weak_ptr_factory_.GetWeakPtr(), gfx::Image()),
        base::TimeDelta::FromSeconds(kDecodeLogoTimeoutSeconds));
    return base::BindRepeating(&ImageDecodedHandlerWithTimeout::OnImageDecoded,
                               handler->weak_ptr_factory_.GetWeakPtr());
  }

 private:
  explicit ImageDecodedHandlerWithTimeout(
      const base::RepeatingCallback<void(const SkBitmap&)>&
          image_decoded_callback)
      : image_decoded_callback_(image_decoded_callback) {}

  void OnImageDecoded(const gfx::Image& decoded_image) {
    image_decoded_callback_.Run(decoded_image.AsBitmap());
    delete this;
  }

  base::RepeatingCallback<void(const SkBitmap&)> image_decoded_callback_;
  base::WeakPtrFactory<ImageDecodedHandlerWithTimeout> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ImageDecodedHandlerWithTimeout);
};

void ObserverOnLogoAvailable(LogoObserver* observer,
                             bool from_cache,
                             LogoCallbackReason type,
                             const base::Optional<Logo>& logo) {
  switch (type) {
    case LogoCallbackReason::DISABLED:
    case LogoCallbackReason::CANCELED:
    case LogoCallbackReason::FAILED:
      break;

    case LogoCallbackReason::REVALIDATED:
      // TODO(sfiera): double-check whether we should inform the observer of the
      // fresh metadata.
      break;

    case LogoCallbackReason::DETERMINED:
      observer->OnLogoAvailable(logo ? &logo.value() : nullptr, from_cache);
      break;
  }
  if (!from_cache) {
    observer->OnObserverRemoved();
  }
}

void RunCallbacksWithDisabled(LogoCallbacks callbacks) {
  if (callbacks.on_cached_encoded_logo_available) {
    std::move(callbacks.on_cached_encoded_logo_available)
        .Run(LogoCallbackReason::DISABLED, base::nullopt);
  }
  if (callbacks.on_cached_decoded_logo_available) {
    std::move(callbacks.on_cached_decoded_logo_available)
        .Run(LogoCallbackReason::DISABLED, base::nullopt);
  }
  if (callbacks.on_fresh_encoded_logo_available) {
    std::move(callbacks.on_fresh_encoded_logo_available)
        .Run(LogoCallbackReason::DISABLED, base::nullopt);
  }
  if (callbacks.on_fresh_decoded_logo_available) {
    std::move(callbacks.on_fresh_decoded_logo_available)
        .Run(LogoCallbackReason::DISABLED, base::nullopt);
  }
}

// Returns whether the metadata for the cached logo indicates that the logo is
// OK to show, i.e. it's not expired or it's allowed to be shown temporarily
// after expiration.
bool IsLogoOkToShow(const LogoMetadata& metadata, base::Time now) {
  base::TimeDelta offset =
      base::TimeDelta::FromMilliseconds(kMaxTimeToLiveMS * 3 / 2);
  base::Time distant_past = now - offset;
  // Sanity check so logos aren't accidentally cached forever.
  if (metadata.expiration_time < distant_past) {
    return false;
  }
  return metadata.can_show_after_expiration || metadata.expiration_time >= now;
}

// Reads the logo from the cache and returns it. Returns NULL if the cache is
// empty, corrupt, expired, or doesn't apply to the current logo URL.
std::unique_ptr<EncodedLogo> GetLogoFromCacheOnFileThread(LogoCache* logo_cache,
                                                          const GURL& logo_url,
                                                          base::Time now) {
  const LogoMetadata* metadata = logo_cache->GetCachedLogoMetadata();
  if (!metadata)
    return nullptr;

  if (metadata->source_url != logo_url || !IsLogoOkToShow(*metadata, now)) {
    logo_cache->SetCachedLogo(nullptr);
    return nullptr;
  }

  return logo_cache->GetCachedLogo();
}

void NotifyAndClear(std::vector<EncodedLogoCallback>* encoded_callbacks,
                    std::vector<LogoCallback>* decoded_callbacks,
                    LogoCallbackReason type,
                    const EncodedLogo* encoded_logo,
                    const Logo* decoded_logo) {
  auto opt_encoded_logo =
      encoded_logo ? base::Optional<EncodedLogo>(*encoded_logo) : base::nullopt;
  for (EncodedLogoCallback& callback : *encoded_callbacks) {
    std::move(callback).Run(type, opt_encoded_logo);
  }
  encoded_callbacks->clear();

  auto opt_decoded_logo =
      decoded_logo ? base::Optional<Logo>(*decoded_logo) : base::nullopt;
  for (LogoCallback& callback : *decoded_callbacks) {
    std::move(callback).Run(type, opt_decoded_logo);
  }
  decoded_callbacks->clear();
}

}  // namespace

LogoServiceImpl::LogoServiceImpl(
    const base::FilePath& cache_directory,
    signin::IdentityManager* identity_manager,
    TemplateURLService* template_url_service,
    std::unique_ptr<image_fetcher::ImageDecoder> image_decoder,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::RepeatingCallback<bool()> want_gray_logo_getter)
    : cache_directory_(cache_directory),
      identity_manager_(identity_manager),
      template_url_service_(template_url_service),
      url_loader_factory_(url_loader_factory),
      want_gray_logo_getter_(std::move(want_gray_logo_getter)),
      image_decoder_(std::move(image_decoder)),
      is_idle_(true),
      is_cached_logo_valid_(false),
      cache_task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      logo_cache_(new LogoCache(cache_directory_),
                  base::OnTaskRunnerDeleter(cache_task_runner_)) {
  identity_manager_->AddObserver(this);
}

LogoServiceImpl::~LogoServiceImpl() = default;

void LogoServiceImpl::Shutdown() {
  // The IdentityManager may be destroyed at any point after Shutdown,
  // so make sure we drop any references to it.
  identity_manager_->RemoveObserver(this);
  ReturnToIdle(kDownloadOutcomeNotTracked);
}

void LogoServiceImpl::GetLogo(search_provider_logos::LogoObserver* observer) {
  LogoCallbacks callbacks;
  callbacks.on_cached_decoded_logo_available =
      base::BindOnce(ObserverOnLogoAvailable, observer, true);
  callbacks.on_fresh_decoded_logo_available =
      base::BindOnce(ObserverOnLogoAvailable, observer, false);
  GetLogo(std::move(callbacks));
}

void LogoServiceImpl::GetLogo(LogoCallbacks callbacks) {
  if (!template_url_service_) {
    RunCallbacksWithDisabled(std::move(callbacks));
    return;
  }

  const TemplateURL* template_url =
      template_url_service_->GetDefaultSearchProvider();
  if (!template_url) {
    RunCallbacksWithDisabled(std::move(callbacks));
    return;
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  GURL logo_url;
  if (command_line->HasSwitch(switches::kSearchProviderLogoURL)) {
    logo_url = GURL(
        command_line->GetSwitchValueASCII(switches::kSearchProviderLogoURL));
  } else {
#if defined(OS_ANDROID)
    // Non-Google default search engine logos are currently enabled only on
    // Android (https://crbug.com/737283).
    logo_url = template_url->logo_url();
#endif
  }

  GURL base_url;
  GURL doodle_url;
  const bool is_google = template_url->url_ref().HasGoogleBaseURLs(
      template_url_service_->search_terms_data());
  if (is_google) {
    // TODO(treib): Put the Google doodle URL into prepopulated_engines.json.
    base_url =
        GURL(template_url_service_->search_terms_data().GoogleBaseURLValue());
    doodle_url = search_provider_logos::GetGoogleDoodleURL(base_url);
  } else {
    if (command_line->HasSwitch(switches::kThirdPartyDoodleURL)) {
      doodle_url = GURL(
          command_line->GetSwitchValueASCII(switches::kThirdPartyDoodleURL));
    } else {
      doodle_url = template_url->doodle_url();
    }
    base_url = doodle_url.GetOrigin();
  }

  if (!logo_url.is_valid() && !doodle_url.is_valid()) {
    RunCallbacksWithDisabled(std::move(callbacks));
    return;
  }

  if (!clock_) {
    clock_ = base::DefaultClock::GetInstance();
  }

  const bool use_fixed_logo = !doodle_url.is_valid();
  if (use_fixed_logo) {
    SetServerAPI(
        logo_url,
        base::BindRepeating(&search_provider_logos::ParseFixedLogoResponse),
        base::BindRepeating(&search_provider_logos::UseFixedLogoUrl));
  } else {
    // We encode the type of doodle (regular or gray) in the URL so that the
    // logo cache gets cleared when that value changes.
    GURL prefilled_url = AppendPreliminaryParamsToDoodleURL(
        want_gray_logo_getter_.Run(), doodle_url);
    SetServerAPI(
        prefilled_url,
        base::Bind(&search_provider_logos::ParseDoodleLogoResponse, base_url),
        base::Bind(&search_provider_logos::AppendFingerprintParamToDoodleURL));
  }

  DCHECK(!logo_url_.is_empty());
  DCHECK(callbacks.on_cached_decoded_logo_available ||
         callbacks.on_cached_encoded_logo_available ||
         callbacks.on_fresh_decoded_logo_available ||
         callbacks.on_fresh_encoded_logo_available);

  if (callbacks.on_cached_encoded_logo_available) {
    on_cached_encoded_logo_.push_back(
        std::move(callbacks.on_cached_encoded_logo_available));
  }
  if (callbacks.on_cached_decoded_logo_available) {
    on_cached_decoded_logo_.push_back(
        std::move(callbacks.on_cached_decoded_logo_available));
  }
  if (callbacks.on_fresh_encoded_logo_available) {
    on_fresh_encoded_logo_.push_back(
        std::move(callbacks.on_fresh_encoded_logo_available));
  }
  if (callbacks.on_fresh_decoded_logo_available) {
    on_fresh_decoded_logo_.push_back(
        std::move(callbacks.on_fresh_decoded_logo_available));
  }

  if (is_idle_) {
    is_idle_ = false;

    base::PostTaskAndReplyWithResult(
        cache_task_runner_.get(), FROM_HERE,
        base::BindRepeating(&GetLogoFromCacheOnFileThread,
                            base::Unretained(logo_cache_.get()), logo_url_,
                            clock_->Now()),
        base::BindRepeating(&LogoServiceImpl::OnCachedLogoRead,
                            weak_ptr_factory_.GetWeakPtr()));
  } else if (is_cached_logo_valid_) {
    NotifyAndClear(&on_cached_encoded_logo_, &on_cached_decoded_logo_,
                   LogoCallbackReason::DETERMINED, cached_encoded_logo_.get(),
                   cached_logo_.get());
  }
}

void LogoServiceImpl::SetLogoCacheForTests(std::unique_ptr<LogoCache> cache) {
  // |logo_cache_| has a custom deleter, which makes the two unique_ptrs
  // be different types. so one can't be moved on top of the other.
  logo_cache_.reset(std::move(cache).release());
}

void LogoServiceImpl::SetClockForTests(base::Clock* clock) {
  clock_ = clock;
}

void LogoServiceImpl::SetServerAPI(
    const GURL& logo_url,
    const ParseLogoResponse& parse_logo_response_func,
    const AppendQueryparamsToLogoURL& append_queryparams_func) {
  if (logo_url == logo_url_)
    return;

  ReturnToIdle(kDownloadOutcomeNotTracked);

  logo_url_ = logo_url;
  parse_logo_response_func_ = parse_logo_response_func;
  append_queryparams_func_ = append_queryparams_func;
}

void LogoServiceImpl::ClearCachedLogo() {
  // First cancel any fetch that might be ongoing.
  ReturnToIdle(kDownloadOutcomeNotTracked);
  // Then clear any cached logo.
  SetCachedLogo(nullptr);
}

void LogoServiceImpl::ReturnToIdle(int outcome) {
  if (outcome != kDownloadOutcomeNotTracked) {
    UMA_HISTOGRAM_ENUMERATION("NewTabPage.LogoDownloadOutcome",
                              static_cast<LogoDownloadOutcome>(outcome),
                              DOWNLOAD_OUTCOME_COUNT);
  }

  // Cancel the current asynchronous operation, if any.
  loader_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Reset state.
  is_idle_ = true;
  cached_logo_.reset();
  cached_encoded_logo_.reset();
  is_cached_logo_valid_ = false;

  // Clear callbacks.
  NotifyAndClear(&on_cached_encoded_logo_, &on_cached_decoded_logo_,
                 LogoCallbackReason::CANCELED, nullptr, nullptr);
  NotifyAndClear(&on_fresh_encoded_logo_, &on_fresh_decoded_logo_,
                 LogoCallbackReason::CANCELED, nullptr, nullptr);
}

void LogoServiceImpl::OnCachedLogoRead(
    std::unique_ptr<EncodedLogo> cached_logo) {
  DCHECK(!is_idle_);

  if (cached_logo && cached_logo->encoded_image) {
    // Store the value of logo->encoded_image for use below. This ensures that
    // logo->encoded_image is evaluated before base::Passed(&logo), which sets
    // logo to NULL.
    scoped_refptr<base::RefCountedString> encoded_image =
        cached_logo->encoded_image;
    image_decoder_->DecodeImage(
        encoded_image->data(), gfx::Size(),  // No particular size desired.
        ImageDecodedHandlerWithTimeout::Wrap(base::BindRepeating(
            &LogoServiceImpl::OnLightCachedImageDecoded,
            weak_ptr_factory_.GetWeakPtr(), base::Passed(&cached_logo))));
  } else if (cached_logo) {
    OnCachedLogoAvailable(std::move(cached_logo), SkBitmap(), SkBitmap());
  } else {
    OnCachedLogoAvailable({}, SkBitmap(), SkBitmap());
  }
}

void LogoServiceImpl::SetCachedLogo(std::unique_ptr<EncodedLogo> logo) {
  cache_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LogoCache::SetCachedLogo,
                                base::Unretained(logo_cache_.get()),
                                base::Owned(logo.release())));
}

void LogoServiceImpl::SetCachedMetadata(const LogoMetadata& metadata) {
  cache_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LogoCache::UpdateCachedLogoMetadata,
                                base::Unretained(logo_cache_.get()), metadata));
}

void LogoServiceImpl::OnLightCachedImageDecoded(
    std::unique_ptr<EncodedLogo> cached_logo,
    const SkBitmap& image) {
  if (cached_logo->metadata.dark_mime_type.empty()) {
    OnCachedLogoAvailable(std::move(cached_logo), image, SkBitmap());
    return;
  }

  // Store the value of logo->dark_encoded_image for use below. This ensures
  // that logo->dark_encoded_image is evaluated before base::Passed(&logo),
  // which sets logo to NULL.
  scoped_refptr<base::RefCountedString> dark_encoded_image =
      cached_logo->dark_encoded_image;

  image_decoder_->DecodeImage(
      dark_encoded_image->data(), gfx::Size(),  // No particular size desired.
      ImageDecodedHandlerWithTimeout::Wrap(base::BindRepeating(
          &LogoServiceImpl::OnCachedLogoAvailable,
          weak_ptr_factory_.GetWeakPtr(), base::Passed(&cached_logo), image)));
}

void LogoServiceImpl::OnCachedLogoAvailable(
    std::unique_ptr<EncodedLogo> encoded_logo,
    const SkBitmap& image,
    const SkBitmap& dark_image) {
  DCHECK(!is_idle_);

  // A dark image is not required, but if one exists (mime type is non-empty)
  // it must be successfully decoded.
  if (encoded_logo && !image.isNull() &&
      (encoded_logo->metadata.dark_mime_type.empty() || !dark_image.isNull())) {
    cached_logo_ = std::make_unique<Logo>();
    cached_logo_->metadata = encoded_logo->metadata;
    cached_logo_->image = image;
    cached_logo_->dark_image = dark_image;
    cached_encoded_logo_ = std::move(encoded_logo);
  }
  is_cached_logo_valid_ = true;
  NotifyAndClear(&on_cached_encoded_logo_, &on_cached_decoded_logo_,
                 LogoCallbackReason::DETERMINED, cached_encoded_logo_.get(),
                 cached_logo_.get());
  FetchLogo();
}

void LogoServiceImpl::FetchLogo() {
  DCHECK(!loader_);
  DCHECK(!is_idle_);

  std::string fingerprint;
  if (cached_logo_ && !cached_logo_->metadata.fingerprint.empty() &&
      cached_logo_->metadata.expiration_time >= clock_->Now()) {
    fingerprint = cached_logo_->metadata.fingerprint;
  }
  GURL url = append_queryparams_func_.Run(logo_url_, fingerprint);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("logo_service", R"(
        semantics {
          sender: "Logo Service"
          description:
            "Provides the logo image (aka Doodle) if Google is your configured "
            "search provider."
          trigger: "Displaying the new tab page on iOS or Android."
          data:
            "Logo ID, and the user's Google cookies to show for example "
            "birthday doodles at appropriate times."
          destination: OTHER
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Choosing a non-Google search engine in Chromium settings under "
            "'Search Engine' will disable this feature."
          policy_exception_justification:
            "Not implemented, considered not useful as it does not upload any"
            "data and just downloads a logo image."
        })");
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&LogoServiceImpl::OnURLLoadComplete,
                     base::Unretained(this), loader_.get()),
      kMaxDownloadBytes);
  logo_download_start_time_ = base::TimeTicks::Now();
}

void LogoServiceImpl::OnFreshLogoParsed(bool* parsing_failed,
                                        bool from_http_cache,
                                        std::unique_ptr<EncodedLogo> logo) {
  DCHECK(!is_idle_);

  if (logo)
    logo->metadata.source_url = logo_url_;

  if (!logo || !logo->encoded_image ||
      (!logo->metadata.dark_mime_type.empty() && !logo->dark_encoded_image)) {
    OnFreshLogoAvailable(std::move(logo), /*download_failed=*/false,
                         *parsing_failed, from_http_cache, SkBitmap(),
                         SkBitmap());
  } else {
    // Store the value of logo->encoded_image for use below. This ensures that
    // logo->encoded_image is evaluated before base::Passed(&logo), which sets
    // logo to NULL.
    scoped_refptr<base::RefCountedString> encoded_image = logo->encoded_image;

    image_decoder_->DecodeImage(
        encoded_image->data(), gfx::Size(),  // No particular size desired.
        ImageDecodedHandlerWithTimeout::Wrap(base::BindRepeating(
            &LogoServiceImpl::OnLightFreshImageDecoded,
            weak_ptr_factory_.GetWeakPtr(), base::Passed(&logo),
            /*download_failed=*/false, *parsing_failed, from_http_cache)));
  }
}

void LogoServiceImpl::OnLightFreshImageDecoded(
    std::unique_ptr<EncodedLogo> logo,
    bool download_failed,
    bool parsing_failed,
    bool from_http_cache,
    const SkBitmap& image) {
  if (logo->metadata.dark_mime_type.empty()) {
    OnFreshLogoAvailable(std::move(logo), download_failed, parsing_failed,
                         from_http_cache, image, SkBitmap());
    return;
  }

  // Store the value of logo->dark_encoded_image for use below. This ensures
  // that logo->encoded_image is evaluated before base::Passed(&logo), which
  // sets logo to NULL.
  scoped_refptr<base::RefCountedString> dark_encoded_image =
      logo->dark_encoded_image;

  image_decoder_->DecodeImage(
      dark_encoded_image->data(), gfx::Size(),  // No particular size desired.
      ImageDecodedHandlerWithTimeout::Wrap(base::BindRepeating(
          &LogoServiceImpl::OnFreshLogoAvailable,
          weak_ptr_factory_.GetWeakPtr(), base::Passed(&logo), download_failed,
          parsing_failed, from_http_cache, image)));
}

void LogoServiceImpl::OnFreshLogoAvailable(
    std::unique_ptr<EncodedLogo> encoded_logo,
    bool download_failed,
    bool parsing_failed,
    bool from_http_cache,
    const SkBitmap& image,
    const SkBitmap& dark_image) {
  DCHECK(!is_idle_);

  LogoDownloadOutcome download_outcome = DOWNLOAD_OUTCOME_COUNT;
  std::unique_ptr<Logo> logo;

  if (download_failed) {
    download_outcome = DOWNLOAD_OUTCOME_DOWNLOAD_FAILED;
  } else if (encoded_logo && !encoded_logo->encoded_image && cached_logo_ &&
             !encoded_logo->metadata.fingerprint.empty() &&
             encoded_logo->metadata.fingerprint ==
                 cached_logo_->metadata.fingerprint) {
    // The cached logo was revalidated, i.e. its fingerprint was verified.
    // mime_type isn't sent when revalidating, so copy it from the cached logo.
    encoded_logo->metadata.mime_type = cached_logo_->metadata.mime_type;
    encoded_logo->metadata.dark_mime_type =
        cached_logo_->metadata.dark_mime_type;
    SetCachedMetadata(encoded_logo->metadata);
    download_outcome = DOWNLOAD_OUTCOME_LOGO_REVALIDATED;
  } else if ((encoded_logo && encoded_logo->encoded_image && image.isNull()) ||
             (encoded_logo && !encoded_logo->metadata.dark_mime_type.empty() &&
              dark_image.isNull())) {
    // Image decoding failed. Do nothing.
    download_outcome = DOWNLOAD_OUTCOME_DECODING_FAILED;
  } else if (encoded_logo && !encoded_logo->encoded_image &&
             encoded_logo->metadata.type != LogoType::INTERACTIVE) {
    download_outcome = DOWNLOAD_OUTCOME_MISSING_REQUIRED_IMAGE;
#if defined(OS_ANDROID) || defined(OS_IOS)
  } else if (encoded_logo && !encoded_logo->encoded_image) {
    // On Mobile interactive doodles require a static CTA image, on Desktop the
    // static image is not required as it's handled by the iframed page.
    download_outcome = DOWNLOAD_OUTCOME_MISSING_REQUIRED_IMAGE;
#endif
  } else {
    // Check if the server returned a valid, non-empty response.
    if (encoded_logo) {
      UMA_HISTOGRAM_BOOLEAN("NewTabPage.LogoImageDownloaded", from_http_cache);

      DCHECK(!encoded_logo->encoded_image || !image.isNull());
      logo.reset(new Logo());
      logo->metadata = encoded_logo->metadata;
      logo->image = image;
      logo->dark_image = dark_image;
    }

    if (logo) {
      download_outcome = DOWNLOAD_OUTCOME_NEW_LOGO_SUCCESS;
    } else {
      if (parsing_failed)
        download_outcome = DOWNLOAD_OUTCOME_PARSING_FAILED;
      else
        download_outcome = DOWNLOAD_OUTCOME_NO_LOGO_TODAY;
    }
  }

  LogoCallbackReason callback_type = LogoCallbackReason::FAILED;
  switch (download_outcome) {
    case DOWNLOAD_OUTCOME_NEW_LOGO_SUCCESS:
      DCHECK(encoded_logo);
      DCHECK(logo);
      callback_type = LogoCallbackReason::DETERMINED;
      break;

    case DOWNLOAD_OUTCOME_PARSING_FAILED:
    case DOWNLOAD_OUTCOME_NO_LOGO_TODAY:
      // Clear the cached logo if it was non-null. Otherwise, report this as a
      // revalidation of "no logo".
      DCHECK(!encoded_logo);
      DCHECK(!logo);
      if (cached_logo_) {
        callback_type = LogoCallbackReason::DETERMINED;
      } else {
        callback_type = LogoCallbackReason::REVALIDATED;
      }
      break;

    case DOWNLOAD_OUTCOME_MISSING_REQUIRED_IMAGE:
    case DOWNLOAD_OUTCOME_DOWNLOAD_FAILED:
      // In the download failed, don't notify the callback at all, since the
      // callback should continue to use the cached logo.
      DCHECK(!encoded_logo || !encoded_logo->encoded_image);
      DCHECK(!logo);
      callback_type = LogoCallbackReason::FAILED;
      break;

    case DOWNLOAD_OUTCOME_DECODING_FAILED:
      DCHECK(encoded_logo);
      DCHECK(!logo);
      encoded_logo.reset();
      callback_type = LogoCallbackReason::FAILED;
      break;

    case DOWNLOAD_OUTCOME_LOGO_REVALIDATED:
      // In the server reported that the cached logo is still current, don't
      // notify the callback at all, since the callback should continue to use
      // the cached logo.
      DCHECK(encoded_logo);
      DCHECK(!logo);
      callback_type = LogoCallbackReason::REVALIDATED;
      break;

    case DOWNLOAD_OUTCOME_COUNT:
      NOTREACHED();
      return;
  }

  NotifyAndClear(&on_fresh_encoded_logo_, &on_fresh_decoded_logo_,
                 callback_type, encoded_logo.get(), logo.get());

  switch (callback_type) {
    case LogoCallbackReason::DETERMINED:
      SetCachedLogo(std::move(encoded_logo));
      break;

    default:
      break;
  }

  ReturnToIdle(download_outcome);
}

void LogoServiceImpl::OnURLLoadComplete(const network::SimpleURLLoader* source,
                                        std::unique_ptr<std::string> body) {
  DCHECK(!is_idle_);
  std::unique_ptr<network::SimpleURLLoader> cleanup_loader(loader_.release());

  if (source->NetError() != net::OK) {
    OnFreshLogoAvailable({}, /*download_failed=*/true, false, false, SkBitmap(),
                         SkBitmap());
    return;
  }

  if (!source->ResponseInfo() || !source->ResponseInfo()->headers ||
      source->ResponseInfo()->headers->response_code() != net::HTTP_OK) {
    OnFreshLogoAvailable({}, /*download_failed=*/true, false, false, SkBitmap(),
                         SkBitmap());
    return;
  }

  UMA_HISTOGRAM_TIMES("NewTabPage.LogoDownloadTime",
                      base::TimeTicks::Now() - logo_download_start_time_);

  std::unique_ptr<std::string> response =
      body ? std::move(body) : std::make_unique<std::string>();
  base::Time response_time = clock_->Now();

  bool from_http_cache = !source->ResponseInfo()->network_accessed;

  bool* parsing_failed = new bool(false);
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(parse_logo_response_func_, std::move(response),
                     response_time, parsing_failed),
      base::BindOnce(&LogoServiceImpl::OnFreshLogoParsed,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Owned(parsing_failed), from_http_cache));
}

void LogoServiceImpl::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo&,
    const GoogleServiceAuthError&) {
  // Clear any cached logo, since it may be personalized (e.g. birthday Doodle).
  if (!clock_) {
    clock_ = base::DefaultClock::GetInstance();
  }
  ClearCachedLogo();
}

}  // namespace search_provider_logos
