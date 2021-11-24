// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/identification_settings_manager.h"

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_clock.h"
#include "chromecast/chromecast_buildflags.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"

namespace chromecast {

namespace {

const char kBackgroundQueryParam[] = "google_cast_bg";
const char kBackgroundQueryVal[] = "true";

#if BUILDFLAG(IS_CAST_AUDIO_ONLY)
// Data URI of PNG file with single white pixel.
const char kImageDataURI[] =
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAA"
    "EAAAABCAAAAAA6fptVAAAACklEQVQYV2P4DwABAQEAWk1v8QAAAABJRU5ErkJggg==";

// CSS URI of a blank text file
const char kCssDataURI[] = "data:text/css,";

// Known image file extensions. Referenced from
// chromium/src/net/base/mime_util.cc for mimetypes equal to image.
const char* kImageFileExtensions[] = {
    "avif",   ".bmp", ".gif", ".ico",  ".jfif", ".jpeg", ".jpg",  ".pjp",
    ".pjpeg", ".png", ".svg", ".svgz", ".tif",  ".tiff", ".webp", ".xbm",
};

// Audio devices are usually with less memory than video devices. For those
// devices, image is not visible to end-user, thus not as important. Ideally we
// want to entirely disable image loading, but that is not possible as some Apps
// depend on events from image loading. To work around it, we keep image loading
// enabled, but replace image URL with data URI of a single-pixel PNG file.
GURL FindReplacementURLAudio(const GURL& gurl) {
  for (const auto* extension : kImageFileExtensions) {
    if (base::EndsWith(gurl.path(), extension,
                       base::CompareCase::INSENSITIVE_ASCII)) {
      LOG(INFO) << "Image (" << extension
                << ") URL detected and replaced with data URI.";
      return GURL(kImageDataURI);
    }
  }

  // Also replace CSS stylesheets with an empty text file to reduce CPU
  // usage by the compositor on devices that don't have GPU acceleration.
  if (base::EndsWith(gurl.path(), ".css",
                     base::CompareCase::INSENSITIVE_ASCII)) {
    LOG(INFO) << "CSS URL detected and replaced with empty text URI.";
    return GURL(kCssDataURI);
  }
  return GURL();
}
#endif  // BUILDFLAG(IS_CAST_AUDIO_ONLY)

bool IsLowerCase(const std::string& str) {
  return str == base::ToLowerASCII(str);
}

std::string CreateKeyStr(const GURL& gurl) {
  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  return base::ToLowerASCII(gurl.ReplaceComponents(replacements).spec());
}

std::string ReplacementMapToString(
    const base::flat_map<std::string, GURL>& replacements) {
  std::string replacement_str;
  for (const auto& it : replacements) {
    replacement_str += base::StringPrintf(
        "\n\"%s\"-> \"%s\",", it.first.c_str(), it.second.spec().c_str());
  }
  return replacement_str;
}

void PossiblyUpdateIsolationInfoAndSiteForCookies(
    network::ResourceRequest* request,
    const GURL& new_url) {
  DCHECK(request);
  DCHECK(new_url.is_valid());
  if (new_url.DeprecatedGetOriginAsURL() ==
          request->url.DeprecatedGetOriginAsURL() ||
      !request->trusted_params.has_value()) {
    return;
  }
  auto new_origin = url::Origin::Create(new_url);
  request->trusted_params->isolation_info =
      net::IsolationInfo::CreateForInternalRequest(new_origin);
  request->site_for_cookies =
      request->trusted_params->isolation_info.site_for_cookies();
}

}  // namespace

IdentificationSettingsManager::SubstitutableParameter
IdentificationSettingsManager::ConvertSubstitutableParameterFromMojom(
    mojom::SubstitutableParameterPtr mojo_param) {
  IdentificationSettingsManager::SubstitutableParameter param;
  param.name = mojo_param->name;
  if (mojo_param->replacement_token.has_value()) {
    param.replacement_token = mojo_param->replacement_token.value();
  }
  if (mojo_param->suppression_token.has_value()) {
    param.suppression_token = mojo_param->suppression_token.value();
  }
  param.is_signature = mojo_param->is_signature;
  param.value = mojo_param->value;
  return param;
}

IdentificationSettingsManager::SubstitutableParameter::
    SubstitutableParameter() = default;
IdentificationSettingsManager::SubstitutableParameter::
    ~SubstitutableParameter() = default;

IdentificationSettingsManager::SubstitutableParameter::SubstitutableParameter(
    const IdentificationSettingsManager::SubstitutableParameter& other) =
    default;
IdentificationSettingsManager::SubstitutableParameter::SubstitutableParameter(
    IdentificationSettingsManager::SubstitutableParameter&& other) noexcept =
    default;

struct IdentificationSettingsManager::RequestInfo {
  RequestInfo(net::HttpRequestHeaders param_headers,
              RequestCompletionCallback param_callback)
      : headers(std::move(param_headers)),
        callback(std::move(param_callback)) {}
  ~RequestInfo() = default;

  net::HttpRequestHeaders headers;
  RequestCompletionCallback callback;
};

IdentificationSettingsManager::IdentificationSettingsManager()
    : next_refresh_time_(base::Time()),
      clock_(base::DefaultClock::GetInstance()) {}

IdentificationSettingsManager::~IdentificationSettingsManager() {}

int IdentificationSettingsManager::WillStartResourceRequest(
    network::ResourceRequest* request,
    const std::string& /* session_id */,
    RequestCompletionCallback callback) {
  int err = net::OK;

  DVLOG(2) << "WillStartResourceRequest: url=" << request->url.spec();

  GURL replacement_url = FindReplacementURL(request->url);
  if (!replacement_url.is_empty()) {
    LOG(INFO) << "WillStartResourceRequest: replacement URL found";
    GURL new_url;
    err = ApplyURLReplacementSettings(request->url, replacement_url, new_url);
    DCHECK_EQ(err, net::OK);
    if (!new_url.is_empty()) {
      PossiblyUpdateIsolationInfoAndSiteForCookies(request, new_url);
      request->url = std::move(new_url);
    }
  }

  if (background_mode_) {
    GURL new_url;
    err = ApplyBackgroundQueryParamIfNeeded(request->url, new_url);
    DCHECK_EQ(err, net::OK);
    if (!new_url.is_empty())
      request->url = std::move(new_url);
  }

  MoveCorsExemptHeaders(&request->headers, &request->cors_exempt_headers);

  for (const auto& header : static_headers_) {
    if (!header.second.empty()) {
      request->cors_exempt_headers.SetHeaderIfMissing(header.first,
                                                      header.second);
    }
  }

  if (IsAllowed(request->url)) {
    LOG(INFO) << "WillStartResourceRequest: allowed url="
              << request->url.spec();
    GURL new_url;
    err = ApplyDeviceIdentificationSettings(request->url, new_url,
                                            &request->cors_exempt_headers);
    if (!new_url.is_empty()) {
      request->url = std::move(new_url);
    }

    if (err != net::OK) {
      if (err == net::ERR_IO_PENDING) {
        pending_requests_.push_back(std::make_unique<RequestInfo>(
            request->cors_exempt_headers, std::move(callback)));
      }
      return err;
    }
  }
  return err;
}

void IdentificationSettingsManager::SetSubstitutableParameters(
    std::vector<mojom::SubstitutableParameterPtr> params) {
  DCHECK_LE(params.size(), 32u);
  // Only set |substitutable_params_| once.
  if (!substitutable_params_.empty()) {
    return;
  }

  std::for_each(
      std::make_move_iterator(params.begin()),
      std::make_move_iterator(params.end()),
      [this](mojom::SubstitutableParameterPtr mojo_param) {
        substitutable_params_.emplace_back(
            ConvertSubstitutableParameterFromMojom(std::move(mojo_param)));
      });
}

void IdentificationSettingsManager::SetClientAuth(
    mojo::PendingRemote<mojom::ClientAuthDelegate> client_auth_delegate) {
  DCHECK(client_auth_delegate);
  if (client_auth_delegate_.is_bound()) {
    return;
  }
  client_auth_delegate_.Bind(std::move(client_auth_delegate));
}

void IdentificationSettingsManager::UpdateAppSettings(
    chromecast::mojom::AppSettingsPtr app_settings) {
  is_allowed_for_device_identification_ =
      app_settings->allowed_for_device_identification;
  allowed_headers_ = app_settings->allowed_headers;
  full_host_names_ = std::move(app_settings->full_host_names);
  wildcard_host_names_ = std::move(app_settings->wildcard_host_names);
}

void IdentificationSettingsManager::UpdateDeviceSettings(
    chromecast::mojom::DeviceSettingsPtr device_settings) {
  static_headers_ = std::move(device_settings->static_headers);
  for (const auto& replacement : device_settings->url_replacements) {
    DCHECK(IsLowerCase(replacement.first.spec()));
    replacements_[replacement.first.spec()] = replacement.second;
  }

  DVLOG(2) << "Updated device settings: url_replacements: "
           << ReplacementMapToString(replacements_);
}

void IdentificationSettingsManager::UpdateSubstitutableParamValues(
    std::vector<chromecast::mojom::IndexValuePairPtr> updated_values) {
  std::for_each(
      std::make_move_iterator(updated_values.begin()),
      std::make_move_iterator(updated_values.end()),
      [this](chromecast::mojom::IndexValuePairPtr index_value_pair) {
        DCHECK(index_value_pair->index < substitutable_params_.size());
        substitutable_params_[index_value_pair->index].value =
            std::move(index_value_pair->value);
      });
}

void IdentificationSettingsManager::UpdateBackgroundMode(bool background_mode) {
  background_mode_ = background_mode;
}

void IdentificationSettingsManager::MoveCorsExemptHeaders(
    net::HttpRequestHeaders* headers,
    net::HttpRequestHeaders* cors_exempt_headers) {
  if (!headers || !cors_exempt_headers) {
    return;
  }
  for (const auto& param : substitutable_params_) {
    if (headers->HasHeader(param.name)) {
      std::string header_value;
      headers->GetHeader(param.name, &header_value);
      cors_exempt_headers->SetHeader(param.name, header_value);
      headers->RemoveHeader(param.name);
    }
  }
}

void IdentificationSettingsManager::HandlePendingRequests() {
  std::vector<std::unique_ptr<RequestInfo>> pending_list;
  pending_list.swap(pending_requests_);
  for (auto& request_info : pending_list) {
    int err = ApplyHeaderChanges(substitutable_params_, &request_info->headers);
    std::move(request_info->callback)
        .Run(err, net::HttpRequestHeaders() /* headers */,
             std::move(request_info->headers) /* cors_exempt_headers */);
  }
}

GURL IdentificationSettingsManager::FindReplacementURL(const GURL& gurl) const {
  DCHECK(gurl.is_valid());
  GURL result;
  std::string url_key = CreateKeyStr(gurl);
  const auto& found = replacements_.find(url_key);
  if (found != replacements_.end())
    result = found->second;

#if BUILDFLAG(IS_CAST_AUDIO_ONLY)
  // Apply audio rule only if no replacement has been applied.
  if (result.is_empty())
    result = FindReplacementURLAudio(gurl);
#endif  // IS_CAST_AUDIO_ONLY

  return result;
}

int IdentificationSettingsManager::ApplyURLReplacementSettings(
    const GURL& request_url,
    const GURL& replacement_url,
    GURL& new_url) const {
  DCHECK(request_url.is_valid());
  if (!replacement_url.is_empty()) {
    ReplaceURL(request_url, replacement_url, new_url);
    if (new_url != request_url) {
      VLOG(1) << "ApplyURLReplacementSettings: Replacing URL: " << request_url;
      VLOG(1) << "ApplyURLReplacementSettings: Replacement: " << new_url.spec();
    }
  }
  return net::OK;
}

void IdentificationSettingsManager::ReplaceURL(const GURL& source,
                                               const GURL& replacement,
                                               GURL& new_url) const {
  DCHECK(source.is_valid());
  DCHECK(replacement.is_valid());
  if (!source.has_query())
    new_url = replacement;
  std::string modified_query(source.query());
  if (replacement.has_query() && !modified_query.empty())
    modified_query.push_back('&');
  modified_query.append(replacement.query());
  GURL::Replacements replacements;
  if (!modified_query.empty())
    replacements.SetQueryStr(modified_query);

  // Append the query string from |source| to |replacement|.
  new_url = replacement.ReplaceComponents(replacements);
}

bool IdentificationSettingsManager::IsAllowed(const GURL& gurl) {
  if (!gurl.SchemeIs("https"))
    return false;
  if (!IsAppAllowedForDeviceIdentification())
    return false;

  const std::string& host_name = base::ToLowerASCII(gurl.host());
  if (base::ranges::find(full_host_names_, host_name) !=
      full_host_names_.end()) {
    return true;
  }

  for (const auto& name : wildcard_host_names_) {
    if (base::EndsWith(host_name, name, base::CompareCase::SENSITIVE))
      return true;
  }

  return false;
}

bool IdentificationSettingsManager::NeedParameter(
    const SubstitutableParameter& param,
    uint32_t index) const {
  bool header_enabled = (allowed_headers_ & (1 << index));
  return param.need_query || (header_enabled && !param.suppress_header);
}

bool IdentificationSettingsManager::NeedsSignature(
    const std::vector<SubstitutableParameter>& parameters) const {
  if (!IsAppAllowedForDeviceIdentification())
    return false;
  uint32_t i = 0;
  for (const auto& param : parameters) {
    if (param.is_signature && NeedParameter(param, i))
      return true;
    ++i;
  }
  return false;
}

void IdentificationSettingsManager::AnalyzeAndReplaceQueryString(
    const GURL& orig_url,
    GURL& new_url,
    std::vector<SubstitutableParameter>* params) {
  base::StringPiece input = orig_url.query_piece();
  std::string output = orig_url.query() + "&";  // Helps with an edge case.
  bool need_replacement = false;
  for (auto& p : *params) {
    if (!p.replacement_token.empty() &&
        input.find(p.replacement_token) != std::string::npos) {
      p.suppress_header = true;
      p.need_query = true;
      need_replacement = true;

      std::string replacement_value = net::EscapeQueryParamValue(p.value, true);
      base::ReplaceSubstringsAfterOffset(&output, 0, p.replacement_token,
                                         replacement_value);
    } else if (!p.suppression_token.empty() &&
               input.find(p.suppression_token) != std::string::npos) {
      p.suppress_header = true;
      need_replacement = true;

      base::ReplaceSubstringsAfterOffset(&output, 0, p.suppression_token, "");
      do {
        // Since |suppression_token| is just a substring match, it may or may
        // not remove an entire query parameter. If it did, make sure & isn't
        // doubled up in the resulting query string.
        base::ReplaceSubstringsAfterOffset(&output, 0, "&&", "&");
      } while (output.find("&&") != std::string::npos);
    }
  }
  if (need_replacement) {
    size_t last_pos = output.size() - 1;
    if (output[last_pos] == '&')
      output.erase(last_pos);
    GURL::Replacements replacements;
    replacements.SetQueryStr(output);
    new_url = orig_url.ReplaceComponents(replacements);
    VLOG(2) << "New url after replacement: " << new_url;
  }
}

void IdentificationSettingsManager::AddHttpHeaders(
    const std::vector<SubstitutableParameter>& parameters,
    net::HttpRequestHeaders* headers) const {
  uint32_t i = 0;
  for (const auto& p : parameters) {
    if (!(allowed_headers_ & (1 << i))) {
      ++i;
      continue;
    }
    if (!p.suppress_header) {
      if (!p.value.empty())
        headers->SetHeaderIfMissing(p.name, p.value);
    }
    ++i;
  }
}

bool IdentificationSettingsManager::IsAppAllowedForDeviceIdentification()
    const {
  return is_allowed_for_device_identification_;
}

int IdentificationSettingsManager::ApplyBackgroundQueryParamIfNeeded(
    const GURL& orig_url,
    GURL& new_url) const {
  // Only append the background query param to http/https queries.
  if (!orig_url.SchemeIsHTTPOrHTTPS()) {
    VLOG(2) << "Ignoring background query param URI with scheme: "
            << orig_url.scheme();
    return net::OK;
  }
  if (background_mode_) {
    new_url = net::AppendOrReplaceQueryParameter(
        orig_url, kBackgroundQueryParam, kBackgroundQueryVal);
  }
  return net::OK;
}

int IdentificationSettingsManager::ApplyDeviceIdentificationSettings(
    const GURL& orig_url,
    GURL& new_url,
    net::HttpRequestHeaders* headers) {
  std::vector<SubstitutableParameter> parameters = substitutable_params_;
  AnalyzeAndReplaceQueryString(orig_url, new_url, &parameters);
  return ApplyHeaderChanges(parameters, headers);
}

int IdentificationSettingsManager::ApplyHeaderChanges(
    const std::vector<SubstitutableParameter>& params,
    net::HttpRequestHeaders* headers) {
  if (NeedsSignature(params)) {
    int err = EnsureSignature();
    if (err != net::OK)
      return err;
  }
  AddHttpHeaders(params, headers);
  return net::OK;
}

int IdentificationSettingsManager::EnsureSignature() {
  DCHECK(IsAppAllowedForDeviceIdentification());
  {
    base::AutoLock lock(lock_);
    // Function can return right away, if the signature is recent.
    base::Time now = clock_->Now();
    if (now < next_refresh_time_)
      return net::OK;

    // Check whether there is a pending request that can be used when
    // it completes.
    if (create_signature_in_progress_)
      return net::ERR_IO_PENDING;

    // From this point, need to actually request a signature.
    int err = EnsureCerts();
    if (err == net::ERR_IO_PENDING)
      return err;
  }
  return CreateSignatureAsync();
}

int IdentificationSettingsManager::CreateSignatureAsync() {
  auto done_signing_callback =
      base::BindOnce(&IdentificationSettingsManager::HandlePendingRequests,
                     base::Unretained(this));
  bool run_done_signing_now;
  {
    base::AutoLock lock(lock_);

    // If another signing is already in progress, just return ERR_IO_PENDING,
    // and the resource request will be paused until the signing work is
    // completed. |done_signing_callback| doesn't have to be cached because the
    // very first one will be triggered from the browser process, which calls
    // HandlePendingRequests and resumes all the pending ResourceRequest's
    // (given the fact that all requests from the same render frame need the
    // same signature).
    if (create_signature_in_progress_) {
      LOG(INFO) << "CreateSignatureAsync in progress.";
      return net::ERR_IO_PENDING;
    }

    // If the signature is fresh enough, just use the current one and run
    // the callback immediately.
    if (clock_->Now() < next_refresh_time_) {
      run_done_signing_now = true;
    } else {
      // Otherwise, create a new signature.
      create_signature_in_progress_ = true;
      run_done_signing_now = false;
    }
  }

  if (run_done_signing_now) {
    std::move(done_signing_callback).Run();
    return net::OK;
  }

  LOG(INFO)
      << "CreateSignatureAsync: about to make asynchronous signing request.";

  // base::Unretained is safe here since |client_auth_delegate_| is a
  // mojo::Remote owned by |this|, any pending calls will be dropped when |this|
  // is destroyed.
  client_auth_delegate_->EnsureSignature(
      base::BindOnce(&IdentificationSettingsManager::SignatureComplete,
                     base::Unretained(this), std::move(done_signing_callback)));
  return net::ERR_IO_PENDING;
}

void IdentificationSettingsManager::SignatureComplete(
    DoneSigningCallback done_signing,
    std::vector<chromecast::mojom::IndexValuePairPtr> signature_headers,
    base::Time next_refresh_time) {
  {
    // |lock_| is always released before the async signing task is posted,
    // so it is safe to re-acquire |lock_| here.
    base::AutoLock lock(lock_);

    if (!signature_headers.empty()) {
      next_refresh_time_ = next_refresh_time;
    }
    UpdateSubstitutableParamValues(std::move(signature_headers));
    create_signature_in_progress_ = false;
  }

  LOG(INFO)
      << "CreateSignatureAsync completed; running all done signing callbacks.";
  std::move(done_signing).Run();
}

int IdentificationSettingsManager::EnsureCerts() {
  lock_.AssertAcquired();
  if (cert_initialized_)
    return net::OK;

  if (create_cert_in_progress_)
    return net::ERR_IO_PENDING;

  create_cert_in_progress_ = true;
  DCHECK(client_auth_delegate_);
  // base::Unretained is safe here since |client_auth_delegate_| is a
  // mojo::Remote owned by |this|, any pending calls will be dropped when
  // |this| is destroyed.
  client_auth_delegate_->EnsureCerts(base::BindOnce(
      &IdentificationSettingsManager::InitCerts, base::Unretained(this)));
  return net::ERR_IO_PENDING;
}

void IdentificationSettingsManager::InitCerts(
    std::vector<chromecast::mojom::IndexValuePairPtr> cert_headers) {
  {
    base::AutoLock lock(lock_);
    UpdateSubstitutableParamValues(std::move(cert_headers));
    create_cert_in_progress_ = false;
    cert_initialized_ = true;
  }
  CreateSignatureAsync();
}

}  // namespace chromecast
