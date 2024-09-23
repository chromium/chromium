// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_READER_REGISTRY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_READER_REGISTRY_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/network/public/cpp/resource_request.h"

namespace web_package {
class SignedWebBundleId;
}  // namespace web_package

namespace web_app {

// A registry to create and keep track of `IsolatedWebAppResponseReader`
// instances used to read Isolated Web Apps. At its core, it contains a map from
// file paths to `IsolatedWebAppResponseReader`s to cache them for repeated
// calls. On non-ChromeOS devices, the first request for a particular file path
// will also check the integrity of the Signed Web Bundle. On ChromeOS, it is
// assumed that the Signed Web Bundle has not been corrupted due to its location
// inside cryptohome, and signatures are not checked.
class IsolatedWebAppReaderRegistry
    : public KeyedService,
      public IwaKeyDistributionInfoProvider::Observer {
 public:
  IsolatedWebAppReaderRegistry(
      Profile& profile,
      std::unique_ptr<IsolatedWebAppResponseReaderFactory> reader_factory);
  ~IsolatedWebAppReaderRegistry() override;

  IsolatedWebAppReaderRegistry(const IsolatedWebAppReaderRegistry&) = delete;
  IsolatedWebAppReaderRegistry& operator=(const IsolatedWebAppReaderRegistry&) =
      delete;

  struct ReadResponseError {
    enum class Type {
      kOtherError,
      kResponseNotFound,
    };

    static ReadResponseError ForError(const UnusableSwbnFileError& error);

    static ReadResponseError ForError(
        const IsolatedWebAppResponseReader::Error& error);

    static ReadResponseError ForOtherError(const std::string& message) {
      return ReadResponseError(Type::kOtherError, message);
    }

    Type type;
    std::string message;

   private:
    static ReadResponseError ForResponseNotFound(const std::string& message) {
      return ReadResponseError(Type::kResponseNotFound, message);
    }

    ReadResponseError(Type type, const std::string& message)
        : type(type), message(message) {}
  };

  using ReadResponseCallback = base::OnceCallback<void(
      base::expected<IsolatedWebAppResponseReader::Response, ReadResponseError>
          response)>;

  // Given a path to a Signed Web Bundle, the expected Signed Web Bundle ID, and
  // a request, read the corresponding response from it. The `callback` receives
  // both the response head and a closure it can call to read the response body,
  // or a string if an error occurs.
  void ReadResponse(const base::FilePath& web_bundle_path,
                    bool dev_mode,
                    const web_package::SignedWebBundleId& web_bundle_id,
                    const network::ResourceRequest& resource_request,
                    ReadResponseCallback callback);

  // Closes the cached readers of the given path. After callback is invoked the
  // caller can expect that the corresponding file is closed.
  void ClearCacheForPath(const base::FilePath& web_bundle_path,
                         base::OnceClosure callback);

  // This enum represents every error type that can occur during response head
  // parsing, after integrity block and metadata have been read successfully.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ReadResponseHeadError {
    kResponseHeadParserInternalError = 1,
    kResponseHeadParserFormatError = 2,
    kResponseNotFoundError = 3,
    kAppNotTrusted = 4,
    kMaxValue = kAppNotTrusted
  };

 private:
  FRIEND_TEST_ALL_PREFIXES(IsolatedWebAppReaderRegistryTest,
                           TestConcurrentRequests);
  FRIEND_TEST_ALL_PREFIXES(IsolatedWebAppReaderRegistryTest,
                           TestSignedWebBundleReaderLifetime);

  // IwaKeyDistributionInfoProvider::Observer:
  void OnComponentUpdateSuccess(
      const base::Version& component_version) override;

  void ClearCacheForPathImpl(const base::FilePath& web_bundle_path,
                             bool dev_mode,
                             base::OnceClosure callback);

  void OnResponseReaderCreated(
      const base::FilePath& web_bundle_path,
      bool dev_mode,
      const web_package::SignedWebBundleId& web_bundle_id,
      base::expected<std::unique_ptr<IsolatedWebAppResponseReader>,
                     UnusableSwbnFileError> reader);

  void DoReadResponse(IsolatedWebAppResponseReader& reader,
                      network::ResourceRequest resource_request,
                      ReadResponseCallback callback);

  void OnResponseRead(
      ReadResponseCallback callback,
      base::expected<IsolatedWebAppResponseReader::Response,
                     IsolatedWebAppResponseReader::Error> response);

  enum class ReaderCacheState;

  // A thin wrapper around `base::flat_map<Cache::Key, Cache::Entry>` that
  // automatically removes entries from the cache if they have not been accessed
  // for some time. This makes sure that `IsolatedWebAppResponseReader`s are not
  // kept alive indefinitely, since each of them holds an open file handle and
  // memory.
  class Cache {
   public:
    struct Key;
    class Entry;

    Cache();
    ~Cache();

    Cache(Cache&& other) = delete;
    Cache& operator=(Cache&& other) = delete;

    base::flat_map<Key, Entry>::iterator Find(const Key& key);

    base::flat_map<Key, Entry>::iterator End();

    template <class... Args>
    std::pair<base::flat_map<Key, Entry>::iterator, bool> Emplace(
        Args&&... args);

    void Erase(base::flat_map<Key, Entry>::iterator iterator);

    struct Key {
      base::FilePath path;
      bool dev_mode;

      bool operator<(const Key& other) const;
    };

    // A cache `Entry` has two states: In its initial `kPending` state, it
    // caches requests made to a Signed Web Bundle until an
    // `IsolatedWebAppResponseReader` is ready. Once the
    // `IsolatedWebAppResponseReader` is ready and set via `set_reader`, all
    // queued requests are run and the state is updated to `kReady`.
    class Entry {
     public:
      Entry();
      ~Entry();

      Entry(const Entry& other) = delete;
      Entry& operator=(const Entry& other) = delete;

      Entry(Entry&& other);
      Entry& operator=(Entry&& other);

      IsolatedWebAppResponseReader& GetReader() {
        DCHECK(reader_);
        last_access_ = base::TimeTicks::Now();
        return *reader_;
      }

      const base::TimeTicks last_access() const { return last_access_; }

      ReaderCacheState AsReaderCacheState() {
        switch (state()) {
          case State::kPending:
            return ReaderCacheState::kCachedPending;
          case State::kReady:
            return ReaderCacheState::kCachedReady;
        }
      }

      enum class State { kPending, kReady };
      State state() const { return reader_ ? State::kReady : State::kPending; }

      bool IsCloseReaderRequested() const;
      void SetCloseReaderCallback(base::OnceClosure callback);
      base::OnceClosure GetCloseReaderCallback();

      std::unique_ptr<IsolatedWebAppResponseReader> StealReader();

      void set_reader(std::unique_ptr<IsolatedWebAppResponseReader> reader) {
        reader_ = std::move(reader);
      }

      std::vector<std::pair<network::ResourceRequest,
                            IsolatedWebAppReaderRegistry::ReadResponseCallback>>
          pending_requests;

     private:
      std::unique_ptr<IsolatedWebAppResponseReader> reader_;
      // The point in time when the `reader` was last accessed.
      base::TimeTicks last_access_;
      base::OnceClosure pending_closed_callback_;
    };

    bool IsCleanupTimerRunningForTesting() const {
      return cleanup_timer_.IsRunning();
    }

   private:
    void StartCleanupTimerIfNotRunning();

    void StopCleanupTimerIfCacheIsEmpty();

    void CleanupOldEntries();

    base::flat_map<Key, Entry> cache_;
    base::RepeatingTimer cleanup_timer_;
    SEQUENCE_CHECKER(sequence_checker_);
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ReaderCacheState {
    kNotCached = 0,
    kCachedReady = 1,
    kCachedPending = 2,
    kMaxValue = kCachedPending
  };

  Cache reader_cache_;

  base::ScopedObservation<IwaKeyDistributionInfoProvider,
                          IwaKeyDistributionInfoProvider::Observer>
      key_distribution_info_observation_{this};

  // A set of files whose signatures have been verified successfully during the
  // current browser session. Signatures of these files are not re-verified even
  // if their corresponding `CacheEntry` is cleaned up and later re-created.
  base::flat_set<base::FilePath> verified_files_;

  const raw_ref<Profile> profile_;

  std::unique_ptr<IsolatedWebAppResponseReaderFactory> reader_factory_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<IsolatedWebAppReaderRegistry> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_READER_REGISTRY_H_
