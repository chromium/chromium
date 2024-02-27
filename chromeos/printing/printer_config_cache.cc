// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/printer_config_cache.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

// Defines the serving root in which all PPDs and PPD metadata reside.
constexpr char kServingRoot[] =
    "https://printerconfigurations.googleusercontent.com/"
    "chromeos_printing/";
constexpr char kLocalhostRoot[] = "http://localhost:7002/";

// Prepends the serving root to |name|, returning the result.
std::string PrependServingRoot(const std::string& name,
                               bool use_localhost_as_root) {
  if (use_localhost_as_root) {
    return base::StrCat({kLocalhostRoot, name});
  }
  return base::StrCat({kServingRoot, name});
}

// Accepts a relative |path| to a value in the Chrome OS Printing
// serving root) and returns a resource request to satisfy the same.
std::unique_ptr<network::ResourceRequest> FormRequest(
    const std::string& path,
    bool use_localhost_as_root) {
  GURL full_url(PrependServingRoot(path, use_localhost_as_root));
  if (!full_url.is_valid()) {
    return nullptr;
  }

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = full_url;

  request->load_flags = net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  return request;
}

}  // namespace

// In case of fetch failure, only the key is meaningful feedback.
// static
PrinterConfigCache::FetchResult PrinterConfigCache::FetchResult::Failure(
    const std::string& key) {
  return PrinterConfigCache::FetchResult{false, key, std::string(),
                                         base::Time()};
}

// static
PrinterConfigCache::FetchResult PrinterConfigCache::FetchResult::Success(
    const std::string& key,
    const std::string& contents,
    base::Time time_of_fetch) {
  return PrinterConfigCache::FetchResult{true, key, contents, time_of_fetch};
}

class PrinterConfigCacheImpl : public PrinterConfigCache {
 public:
  explicit PrinterConfigCacheImpl(
      const base::Clock* clock,
      base::RepeatingCallback<network::mojom::URLLoaderFactory*()>
          loader_factory_dispenser,
      bool use_localhost_as_root)
      : clock_(clock),
        loader_factory_dispenser_(std::move(loader_factory_dispenser)),
        use_localhost_as_root_(use_localhost_as_root),
        weak_factory_(this) {}

  ~PrinterConfigCacheImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void Fetch(const std::string& key,
             base::TimeDelta expiration,
             FetchCallback cb) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Try to answer this fetch request locally.
    const auto& finding = cache_.find(key);
    if (finding != cache_.end()) {
      const Entry& entry = finding->second;
      if (entry.time_of_fetch + expiration > clock_->Now()) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(cb), FetchResult::Success(
                                                         key, entry.contents,
                                                         entry.time_of_fetch)));
        return;
      }
    }

    // We couldn't answer this request locally. Issue a networked fetch
    // and defer the answer to when we hear back.
    auto context = std::make_unique<FetchContext>(key, std::move(cb));
    fetch_queue_.push(std::move(context));
    TryToStartNetworkedFetch();
  }

  void Drop(const std::string& key) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    cache_.erase(key);
  }

 private:
  // A FetchContext saves off the key and the FetchCallback that a
  // caller passes to PrinterConfigCacheImpl::Fetch().
  struct FetchContext {
    const std::string key;
    PrinterConfigCache::FetchCallback cb;

    FetchContext(const std::string& arg_key,
                 PrinterConfigCache::FetchCallback arg_cb)
        : key(arg_key), cb(std::move(arg_cb)) {}
    ~FetchContext() = default;
  };

  // If a PrinterConfigCache maps keys to values, then Entry structs
  // represent values.
  struct Entry {
    std::string contents;
    base::Time time_of_fetch;

    Entry(const std::string& arg_contents, base::Time time)
        : contents(arg_contents), time_of_fetch(time) {}
    ~Entry() = default;
  };

  void TryToStartNetworkedFetch() {
    // Either
    // 1. a networked fetch is already in flight or
    // 2. there are no more pending networked fetches to act upon.
    // In either case, we can't do anything at the moment; back off
    // and let a future call to Fetch() or FinishNetworkedFetch()
    // return here to try again.
    if (fetcher_ || fetch_queue_.empty()) {
      return;
    }

    std::unique_ptr<FetchContext> context = std::move(fetch_queue_.front());
    fetch_queue_.pop();
    auto request = FormRequest(context->key, use_localhost_as_root_);

    // Create traffic annotation tag.
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("printer_config_fetch", R"(
          semantics {
            sender: "Printer Configuration"
            description:
              "This component sends requests to the Chrome OS Printing "
              "serving root during printer configuration. This can return "
              "two pieces of information, depending on the request: "
              "PostScript Printer Description (PPD) files for a specified "
              "printer, and PPD file metadata to help locate the desired PPD "
              "file."
            trigger: "On printer setup in ChromeOS."
            data: "Printer names (comprising of make and/or model)."
            user_data: {
              type: OTHER
            }
            destination: GOOGLE_OWNED_SERVICE
            internal: {
              contacts: {
                email: "bmgordon@google.com"
              }
            }
            last_reviewed: "2023-01-18"
          }
          policy {
            cookies_allowed: NO
            setting:
              "Admins must disable access to both enterprise and "
              "non-enterprise printers. Enterprise printers should be left "
              "empty under 'Devices > Chrome > Printers'. Non-enterprise "
              "printers can be disabled under 'Devices > Chrome > Settings > "
              "Printer management' by setting to: 'Do not allow users to add "
              "new printers'."
            chrome_policy {
              UserPrintersAllowed {
                UserPrintersAllowed: false
              }
              PrintersBulkConfiguration: {
                PrintersBulkConfiguration: ""
              }
            }
            chrome_device_policy {
              # DevicePrinters
              device_printers: {
                external_policy: ""
              }
            }
          })");
    fetcher_ = network::SimpleURLLoader::Create(std::move(request),
                                                traffic_annotation);

    fetcher_->DownloadToString(
        loader_factory_dispenser_.Run(),
        base::BindOnce(&PrinterConfigCacheImpl::FinishNetworkedFetch,
                       weak_factory_.GetWeakPtr(), std::move(context)),
        network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
  }

  // Called by |fetcher_| once DownloadToString() completes.
  void FinishNetworkedFetch(std::unique_ptr<FetchContext> context,
                            std::unique_ptr<std::string> contents) {
    // Wherever |fetcher_| works its sorcery, it had better have posted
    // back onto _our_ sequence.
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (fetcher_->NetError() == net::Error::OK) {
      // We only want to update our local cache if the |fetcher_|
      // succeeded; otherwise, prefer to either retain the stale entry
      // (if extant) or retain no entry at all (if not).
      const Entry newly_inserted = Entry(*contents, clock_->Now());
      cache_.insert_or_assign(context->key, newly_inserted);
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(context->cb),
                                    FetchResult::Success(
                                        context->key, newly_inserted.contents,
                                        newly_inserted.time_of_fetch)));
    } else {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(context->cb),
                                    FetchResult::Failure(context->key)));
    }

    fetcher_.reset();
    TryToStartNetworkedFetch();
  }

  // The heart of an PrinterConfigCache: the local cache itself.
  base::flat_map<std::string, Entry> cache_;

  // Enqueues networked requests.
  base::queue<std::unique_ptr<FetchContext>> fetch_queue_;

  // Dispenses Time objects to mark time of fetch on Entry instances.
  raw_ptr<const base::Clock> clock_;

  // Dispenses fresh URLLoaderFactory instances; see header comment
  // on Create().
  base::RepeatingCallback<network::mojom::URLLoaderFactory*()>
      loader_factory_dispenser_;

  // Talks to the networked service to fetch resources.
  //
  // Because this class is sequenced, a non-nullptr value here (observed
  // on-sequence) denotes an ongoing fetch. See the
  // TryToStartNetworkedFetch() and FinishNetworkedFetch() methods.
  std::unique_ptr<network::SimpleURLLoader> fetcher_;

  // Determines the address of the server.
  const bool use_localhost_as_root_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Dispenses weak pointers to our |fetcher_|. This is necessary
  // because |this| could be deleted while the loader is in flight
  // off-sequence.
  base::WeakPtrFactory<PrinterConfigCacheImpl> weak_factory_;
};

// static
std::unique_ptr<PrinterConfigCache> PrinterConfigCache::Create(
    const base::Clock* clock,
    base::RepeatingCallback<network::mojom::URLLoaderFactory*()>
        loader_factory_dispenser,
    bool use_localhost_as_root) {
  return std::make_unique<PrinterConfigCacheImpl>(
      clock, std::move(loader_factory_dispenser), use_localhost_as_root);
}

}  // namespace chromeos
