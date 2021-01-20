// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_IN_PROGRESS_DOWNLOAD_MANAGER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_IN_PROGRESS_DOWNLOAD_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_file_factory.h"
#include "components/download/public/common/download_item_impl_delegate.h"
#include "components/download/public/common/download_job.h"
#include "components/download/public/common/download_utils.h"
#include "components/download/public/common/simple_download_manager.h"
#include "components/download/public/common/url_download_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace download {

class DownloadDBCache;
class DownloadStartObserver;
class DownloadUrlParameters;
struct DownloadDBEntry;

// Manager for handling all active downloads.
class COMPONENTS_DOWNLOAD_EXPORT InProgressDownloadManager
    : public UrlDownloadHandler::Delegate,
      public DownloadItemImplDelegate,
      public SimpleDownloadManager {
 public:
  using StartDownloadItemCallback =
      base::OnceCallback<void(std::unique_ptr<DownloadCreateInfo> info,
                              DownloadItemImpl*,
                              bool /* should_persist_new_download */)>;
  using DisplayNames = std::unique_ptr<
      std::map<std::string /*content URI*/, base::FilePath /* display name*/>>;

  // Class to be notified when download starts/stops.
  class COMPONENTS_DOWNLOAD_EXPORT Delegate {
   public:
    // Called when in-progress downloads are initialized.
    virtual void OnDownloadsInitialized() {}

    // Intercepts the download to another system if applicable. Returns true if
    // the download was intercepted.
    virtual bool InterceptDownload(
        const DownloadCreateInfo& download_create_info);

    // Gets the default download directory.
    virtual base::FilePath GetDefaultDownloadDirectory();

    // Gets the download item for the given |download_create_info|.
    // TODO(qinmin): remove this method and let InProgressDownloadManager
    // create the DownloadItem from in-progress cache.
    virtual void StartDownloadItem(
        std::unique_ptr<DownloadCreateInfo> info,
        DownloadUrlParameters::OnStartedCallback on_started,
        StartDownloadItemCallback callback) {}
  };

  using IsOriginSecureCallback = base::RepeatingCallback<bool(const GURL&)>;
  using WakeLockProviderBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<device::mojom::WakeLockProvider>)>;
  // Creates a new InProgressDownloadManager instance. If |in_progress_db_dir|
  // is empty then it will use an empty database and no history will be saved.
  // |db_provider| can be nullptr if |in_progress_db_dir| is empty.
  // |wake_lock_provider_binder| may be null.
  InProgressDownloadManager(Delegate* delegate,
                            const base::FilePath& in_progress_db_dir,
                            leveldb_proto::ProtoDatabaseProvider* db_provider,
                            const IsOriginSecureCallback& is_origin_secure_cb,
                            const URLSecurityPolicy& url_security_policy,
                            WakeLockProviderBinder wake_lock_provider_binder);
  ~InProgressDownloadManager() override;

  // SimpleDownloadManager implementation.
  void DownloadUrl(std::unique_ptr<DownloadUrlParameters> params) override;
  bool CanDownload(DownloadUrlParameters* params) override;
  void GetAllDownloads(
      SimpleDownloadManager::DownloadVector* downloads) override;
  DownloadItem* GetDownloadByGuid(const std::string& guid) override;

  // Called to start a download.
  void BeginDownload(std::unique_ptr<DownloadUrlParameters> params,
                     std::unique_ptr<network::PendingSharedURLLoaderFactory>
                         pending_url_loader_factory,
                     bool is_new_download,
                     const GURL& site_url,
                     const GURL& tab_url,
                     const GURL& tab_referrer_url);

  // Intercepts a download from navigation.
  void InterceptDownloadFromNavigation(
      std::unique_ptr<network::ResourceRequest> resource_request,
      int render_process_id,
      int render_frame_id,
      const GURL& site_url,
      const GURL& tab_url,
      const GURL& tab_referrer_url,
      std::vector<GURL> url_chain,
      net::CertStatus cert_status,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory);

  void StartDownload(std::unique_ptr<DownloadCreateInfo> info,
                     std::unique_ptr<InputStream> stream,
                     URLLoaderFactoryProvider::URLLoaderFactoryProviderPtr
                         url_loader_factory_provider,
                     DownloadJob::CancelRequestCallback cancel_request_callback,
                     DownloadUrlParameters::OnStartedCallback on_started);

  // Shutting down the manager and stop all downloads.
  void ShutDown();

  // DownloadItemImplDelegate implementations.
  void DetermineDownloadTarget(DownloadItemImpl* download,
                               DownloadTargetCallback callback) override;
  void ResumeInterruptedDownload(std::unique_ptr<DownloadUrlParameters> params,
                                 const GURL& site_url) override;
  bool ShouldOpenDownload(DownloadItemImpl* item,
                          ShouldOpenDownloadCallback callback) override;
  void ReportBytesWasted(DownloadItemImpl* download) override;

  // Called to remove an in-progress download.
  void RemoveInProgressDownload(const std::string& guid);

  void set_download_start_observer(DownloadStartObserver* observer) {
    download_start_observer_ = observer;
  }

#if defined(OS_ANDROID)
  // Callback to generate an intermediate file path from the given target file
  // path;
  using IntermediatePathCallback =
      base::RepeatingCallback<base::FilePath(const base::FilePath&)>;
  void set_intermediate_path_cb(
      const IntermediatePathCallback& intermediate_path_cb) {
    intermediate_path_cb_ = intermediate_path_cb;
  }

  void set_default_download_dir(base::FilePath default_download_dir) {
    default_download_dir_ = default_download_dir;
  }
#endif

  // Called to get all in-progress DownloadItemImpl.
  // TODO(qinmin): remove this method once InProgressDownloadManager owns
  // all in-progress downloads.
  virtual std::vector<std::unique_ptr<download::DownloadItemImpl>>
  TakeInProgressDownloads();

  // Called when all the DownloadItem is loaded.
  // TODO(qinmin): remove this once features::kDownloadDBForNewDownloads is
  // enabled by default.
  void OnAllInprogressDownloadsLoaded();

  // Gets the display name for a download. For non-android platforms, this
  // always returns an empty path.
  base::FilePath GetDownloadDisplayName(const base::FilePath& path);

  void set_file_factory(std::unique_ptr<DownloadFileFactory> file_factory) {
    file_factory_ = std::move(file_factory);
  }
  DownloadFileFactory* file_factory() { return file_factory_.get(); }

  void set_url_loader_factory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
    url_loader_factory_ = url_loader_factory;
  }

  void SetDelegate(Delegate* delegate);

  void set_is_origin_secure_cb(
      const IsOriginSecureCallback& is_origin_secure_cb) {
    is_origin_secure_cb_ = is_origin_secure_cb;
  }

  // Called to insert an in-progress download for testing purpose.
  void AddInProgressDownloadForTest(
      std::unique_ptr<download::DownloadItemImpl> download);

 private:
  void Initialize(const base::FilePath& in_progress_db_dir,
                  leveldb_proto::ProtoDatabaseProvider* db_provider);

  // UrlDownloadHandler::Delegate implementations.
  void OnUrlDownloadStarted(
      std::unique_ptr<DownloadCreateInfo> download_create_info,
      std::unique_ptr<InputStream> input_stream,
      URLLoaderFactoryProvider::URLLoaderFactoryProviderPtr
          url_loader_factory_provider,
      UrlDownloadHandler* downloader,
      DownloadUrlParameters::OnStartedCallback callback) override;
  void OnUrlDownloadStopped(UrlDownloadHandler* downloader) override;
  void OnUrlDownloadHandlerCreated(
      UrlDownloadHandler::UniqueUrlDownloadHandlerPtr downloader) override;

  // Called when the in-progress DB is initialized.
  void OnDBInitialized(bool success,
                       std::unique_ptr<std::vector<DownloadDBEntry>> entries);

  // Called when download display names are retrieved,
  void OnDownloadNamesRetrieved(
      std::unique_ptr<std::vector<DownloadDBEntry>> entries,
      DisplayNames display_names);

  // Start a DownloadItemImpl.
  void StartDownloadWithItem(
      std::unique_ptr<InputStream> stream,
      URLLoaderFactoryProvider::URLLoaderFactoryProviderPtr
          url_loader_factory_provider,
      DownloadJob::CancelRequestCallback cancel_request_callback,
      std::unique_ptr<DownloadCreateInfo> info,
      DownloadItemImpl* download,
      bool should_persist_new_download);

  // Called when downloads are initialized.
  void OnDownloadsInitialized();

  // Called to notify |delegate_| that downloads are initialized.
  void NotifyDownloadsInitialized();

  // Cancels the given UrlDownloadHandler.
  void CancelUrlDownload(UrlDownloadHandler* downloader, bool user_cancel);

  // Active download handlers.
  std::vector<UrlDownloadHandler::UniqueUrlDownloadHandlerPtr>
      url_download_handlers_;

  // Delegate to provide information to create a new download. Can be null.
  Delegate* delegate_;

  // Factory for the creation of download files.
  std::unique_ptr<DownloadFileFactory> file_factory_;

  // Cache for DownloadDB.
  std::unique_ptr<DownloadDBCache> download_db_cache_;


  // listens to information about in-progress download items.
  std::unique_ptr<DownloadItem::Observer> in_progress_download_observer_;

  // Observer to notify when a download starts.
  DownloadStartObserver* download_start_observer_;

  // callback to check if an origin is secure.
  IsOriginSecureCallback is_origin_secure_cb_;

#if defined(OS_ANDROID)
  // Callback to generate the intermediate file path.
  IntermediatePathCallback intermediate_path_cb_;

  // Default download directory.
  base::FilePath default_download_dir_;
#endif

  // A list of in-progress download items, could be null if DownloadManagerImpl
  // is managing all downloads.
  std::vector<std::unique_ptr<DownloadItemImpl>> in_progress_downloads_;

  // A list of download GUIDs that should not be persisted.
  std::set<std::string> non_persistent_download_guids_;

  // URLLoaderFactory for issuing network request when DownloadManagerImpl
  // is not available.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Mapping between download URIs and display names.
  // TODO(qinmin): move display name to history and in-progress DB.
  DisplayNames display_names_;

  // Used to check if the URL is safe.
  URLSecurityPolicy url_security_policy_;

  // Callback used to bind WakeLockProvider receivers, if not null.
  const WakeLockProviderBinder wake_lock_provider_binder_;

  base::WeakPtrFactory<InProgressDownloadManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InProgressDownloadManager);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_IN_PROGRESS_DOWNLOAD_MANAGER_H_
