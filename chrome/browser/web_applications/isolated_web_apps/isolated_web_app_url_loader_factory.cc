// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_loader_factory.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_apply_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_reader_registry_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/pending_install_info.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/web_bundle_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_completion_status.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

constexpr char kInstallPagePath[] = "/.well-known/_generated_install_page.html";
constexpr char kInstallPageContent[] = R"(
    <!DOCTYPE html>
    <html>
      <head>
        <meta charset="utf-8" />
        <meta http-equiv="Content-Security-Policy" content="default-src 'self'">
        <link rel="manifest" href="/.well-known/manifest.webmanifest" />
      </head>
    </html>
)";

constexpr char kIsolatedAppCspTemplate[] =
    "base-uri 'none';"
    "default-src 'self';"
    "object-src 'none';"
    "frame-src 'self' https: blob: data:;"
    "connect-src 'self' https: wss: blob: data:%s;"
    "script-src 'self' 'wasm-unsafe-eval';"
    "img-src 'self' https: blob: data:;"
    "media-src 'self' https: blob: data:;"
    "font-src 'self' blob: data:;"
    "style-src 'self' 'unsafe-inline';"
    "require-trusted-types-for 'script';"
    "frame-ancestors 'self';";

bool IsSupportedHttpMethod(const std::string& method) {
  return method == net::HttpRequestHeaders::kGetMethod ||
         method == net::HttpRequestHeaders::kHeadMethod;
}

const std::string& GetDefaultCsp() {
  static const base::NoDestructor<std::string> default_csp(
      [] { return base::StringPrintf(kIsolatedAppCspTemplate, ""); }());
  return *default_csp;
}

std::optional<std::string> ComputeCspOverride(const IwaSourceWithMode& source) {
  auto* proxy_source = absl::get_if<IwaSourceProxy>(&source.variant());
  if (proxy_source && proxy_source->proxy_url().scheme() == "http") {
    url::Origin origin = proxy_source->proxy_url();
    std::string proxy_ws_url =
        base::StringPrintf(" ws://%s:%i", origin.host().c_str(), origin.port());
    return base::StringPrintf(kIsolatedAppCspTemplate, proxy_ws_url.c_str());
  }
  return std::nullopt;
}

void CompleteWithGeneratedResponse(
    mojo::Remote<network::mojom::URLLoaderClient> loader_client,
    net::HttpStatusCode http_status_code,
    std::optional<std::string> body = std::nullopt,
    std::string_view content_type = "text/html") {
  size_t content_length = body.has_value() ? body->size() : 0;
  std::string headers = base::StringPrintf(
      "HTTP/1.1 %d %s\n"
      "Content-Type: %s;charset=utf-8\n"
      "Content-Length: %s\n\n",
      static_cast<int>(http_status_code),
      net::GetHttpReasonPhrase(http_status_code), content_type.data(),
      base::NumberToString(content_length).c_str());
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  response_head->headers->GetMimeTypeAndCharset(&response_head->mime_type,
                                                &response_head->charset);
  response_head->content_length = content_length;

  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  mojo::ScopedDataPipeProducerHandle producer_handle;

  auto result = mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle);
  if (result != MOJO_RESULT_OK) {
    loader_client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    return;
  }

  loader_client->OnReceiveResponse(std::move(response_head),
                                   std::move(consumer_handle),
                                   /*cached_metadata=*/std::nullopt);

  if (body.has_value()) {
    size_t actually_written_bytes = 0;
    MojoResult write_result = producer_handle->WriteData(
        base::as_byte_span(*body), MOJO_WRITE_DATA_FLAG_NONE,
        actually_written_bytes);
    if (write_result != MOJO_RESULT_OK ||
        actually_written_bytes != body->size()) {
      loader_client->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }
  } else {
    producer_handle.reset();
  }

  network::URLLoaderCompletionStatus status(net::OK);
  status.encoded_data_length = headers.size() + content_length;
  status.encoded_body_length = content_length;
  status.decoded_body_length = content_length;
  loader_client->OnComplete(status);
}

void LogErrorMessageToConsole(
    std::optional<content::FrameTreeNodeId> frame_tree_node_id,
    const std::string& error_message) {
  if (!frame_tree_node_id.has_value()) {
    LOG(ERROR) << error_message;
    return;
  }
  // TODO(crbug.com/40239529): The console message will vanish from the console
  // if the user does not have the `Preserve Log` option enabled, since it is
  // triggered before the navigation commits. We should try to use a similar
  // approach as in crrev.com/c/3397976, but `FrameTreeNode` is not part of
  // content/public.

  // Find the `RenderFrameHost` associated with the `FrameTreeNode`
  // corresponding to the `frame_tree_node_id`, and then log the message.
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(*frame_tree_node_id);
  if (!web_contents) {
    // Log to the terminal if we can't log to the console.
    LOG(ERROR) << error_message;
    return;
  }

  web_contents->ForEachRenderFrameHostWithAction(
      [frame_tree_node_id,
       &error_message](content::RenderFrameHost* render_frame_host) {
        if (render_frame_host->GetFrameTreeNodeId() == frame_tree_node_id) {
          render_frame_host->AddMessageToConsole(
              blink::mojom::ConsoleMessageLevel::kError, error_message);
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
}

base::expected<std::reference_wrapper<const WebApp>, std::string>
FindIsolatedWebApp(WebAppProvider& provider,
                   const IsolatedWebAppUrlInfo& url_info) {
  const WebAppRegistrar& registrar = provider.registrar_unsafe();

  if (!registrar.IsInstallState(
          url_info.app_id(),
          {proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
           proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION})) {
    return base::unexpected("Isolated Web App not installed: " +
                            url_info.origin().Serialize());
  }
  const WebApp* iwa = registrar.GetAppById(url_info.app_id());

  if (!iwa->isolation_data().has_value()) {
    return base::unexpected("App is not an Isolated Web App: " +
                            url_info.origin().Serialize());
  }

  return *iwa;
}

class HeaderInjectionURLLoaderClient : public network::mojom::URLLoaderClient {
 public:
  explicit HeaderInjectionURLLoaderClient(
      mojo::PendingRemote<network::mojom::URLLoaderClient> url_loader_client)
      : url_loader_client_(std::move(url_loader_client)) {}

  void set_csp_override(std::string_view csp_override) {
    csp_override_ = csp_override;
  }

 private:
  // `network::mojom::URLLoaderClient`:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnReceiveEarlyHints(std::move(early_hints));
  }

  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
    DCHECK(url_loader_client_.is_bound());

    scoped_refptr<net::HttpResponseHeaders> headers = response_head->headers;
    size_t original_size = headers->raw_headers().size();

    std::string csp_header =
        csp_override_.has_value() ? csp_override_.value() : GetDefaultCsp();

    // Apps could specify a more restrictive CSP than what we enforce, which we
    // don't want to overwrite. We add our CSP here so that existing CSPs will
    // still be enforced. Existing CO*P headers are replaced.
    headers->AddHeader("Content-Security-Policy", csp_header);
    headers->SetHeader("Cross-Origin-Opener-Policy", "same-origin");
    headers->SetHeader("Cross-Origin-Embedder-Policy", "require-corp");
    headers->SetHeader("Cross-Origin-Resource-Policy", "same-origin");

    header_size_delta_ = headers->raw_headers().size() - original_size;

    // The Network Service will have already parsed the headers for proxy-based
    // IWAs, and navigation code will try to reuse the already parsed headers
    // if they're available. However, we're modifying the headers so we want
    // them to be re-parsed. This re-parsing requires an additional round-trip
    // to the Network Service.
    response_head->parsed_headers = nullptr;

    url_loader_client_->OnReceiveResponse(
        std::move(response_head), std::move(body), std::move(cached_metadata));
  }

  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnReceiveRedirect(redirect_info,
                                          std::move(response_head));
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnUploadProgress(current_position, total_size,
                                         std::move(ack_callback));
  }

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnTransferSizeUpdated(transfer_size_diff);
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    DCHECK(url_loader_client_.is_bound());
    network::URLLoaderCompletionStatus adjusted_status = status;
    adjusted_status.encoded_data_length += header_size_delta_;
    url_loader_client_->OnComplete(adjusted_status);
  }

  mojo::Remote<network::mojom::URLLoaderClient> url_loader_client_;
  std::optional<std::string> csp_override_ = std::nullopt;
  int header_size_delta_ = 0;
};

class IsolatedWebAppURLLoader : public network::mojom::URLLoader {
 public:
  IsolatedWebAppURLLoader(
      IsolatedWebAppReaderRegistry* isolated_web_app_reader_registry,
      const base::FilePath& web_bundle_path,
      bool dev_mode,
      web_package::SignedWebBundleId web_bundle_id,
      mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client,
      const network::ResourceRequest& resource_request,
      std::optional<content::FrameTreeNodeId> frame_tree_node_id)
      : loader_client_(std::move(loader_client)),
        resource_request_(resource_request),
        frame_tree_node_id_(frame_tree_node_id) {
    isolated_web_app_reader_registry->ReadResponse(
        web_bundle_path, dev_mode, web_bundle_id, resource_request,
        base::BindOnce(&IsolatedWebAppURLLoader::OnResponseRead,
                       weak_factory_.GetWeakPtr()));
  }

  IsolatedWebAppURLLoader(const IsolatedWebAppURLLoader&) = delete;
  IsolatedWebAppURLLoader& operator=(const IsolatedWebAppURLLoader&) = delete;
  IsolatedWebAppURLLoader(IsolatedWebAppURLLoader&&) = delete;
  IsolatedWebAppURLLoader& operator=(IsolatedWebAppURLLoader&&) = delete;

 private:
  void OnResponseRead(
      base::expected<IsolatedWebAppResponseReader::Response,
                     IsolatedWebAppReaderRegistry::ReadResponseError>
          response) {
    if (!loader_client_.is_connected()) {
      return;
    }

    if (!response.has_value()) {
      LogErrorMessageToConsole(frame_tree_node_id_, response.error().message);
      switch (response.error().type) {
        case IsolatedWebAppReaderRegistry::ReadResponseError::Type::kOtherError:
          loader_client_->OnComplete(
              network::URLLoaderCompletionStatus(net::ERR_INVALID_WEB_BUNDLE));
          return;
        case IsolatedWebAppReaderRegistry::ReadResponseError::Type::
            kResponseNotFound:
          // Return a synthetic 404 response.
          CompleteWithGeneratedResponse(std::move(loader_client_),
                                        net::HTTP_NOT_FOUND);
          return;
      }
    }

    // TODO(crbug.com/41474458): For the initial implementation, we allow only
    // net::HTTP_OK, but we should clarify acceptable status code in the spec.
    if (response->head()->response_code != net::HTTP_OK) {
      LogErrorMessageToConsole(
          frame_tree_node_id_,
          base::StringPrintf(
              "Failed to read response from Signed Web Bundle: The response "
              "has an unsupported HTTP status code: %d (only status code %d is "
              "allowed).",
              response->head()->response_code, net::HTTP_OK));
      loader_client_->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_INVALID_WEB_BUNDLE));
      return;
    }

    std::string header_string =
        web_package::CreateHeaderString(response->head());
    auto response_head =
        web_package::CreateResourceResponseFromHeaderString(header_string);
    response_head->content_length = response->head()->payload_length;
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = std::min(
        uint64_t{network::features::GetDataPipeDefaultAllocationSize()},
        response->head()->payload_length);

    auto result =
        mojo::CreateDataPipe(&options, producer_handle, consumer_handle);
    if (result != MOJO_RESULT_OK) {
      loader_client_->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
      return;
    }
    header_length_ = header_string.size();
    body_length_ = response_head->content_length;
    loader_client_->OnReceiveResponse(std::move(response_head),
                                      std::move(consumer_handle), std::nullopt);

    response->ReadBody(
        std::move(producer_handle),
        base::BindOnce(&IsolatedWebAppURLLoader::FinishReadingBody,
                       weak_factory_.GetWeakPtr()));
  }

  void FinishReadingBody(net::Error net_error) {
    if (!loader_client_.is_connected()) {
      return;
    }

    network::URLLoaderCompletionStatus status(net_error);
    // For these values we use the same `body_length_` as we don't currently
    // provide encoding in Web Bundles.
    status.encoded_data_length = body_length_ + header_length_;
    status.encoded_body_length = body_length_;
    status.decoded_body_length = body_length_;
    loader_client_->OnComplete(status);
  }

  // network::mojom::URLLoader implementation
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override {
    NOTREACHED_IN_MIGRATION();
  }
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

  mojo::Remote<network::mojom::URLLoaderClient> loader_client_;
  int64_t header_length_;
  int64_t body_length_;
  const network::ResourceRequest resource_request_;
  std::optional<content::FrameTreeNodeId> frame_tree_node_id_;

  base::WeakPtrFactory<IsolatedWebAppURLLoader> weak_factory_{this};
};

}  // namespace

IsolatedWebAppURLLoaderFactory::IsolatedWebAppURLLoaderFactory(
    Profile* profile,
    std::optional<url::Origin> app_origin,
    std::optional<content::FrameTreeNodeId> frame_tree_node_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver)
    : network::SelfDeletingURLLoaderFactory(std::move(factory_receiver)),
      profile_(profile),
      app_origin_(std::move(app_origin)),
      frame_tree_node_id_(frame_tree_node_id) {
  CHECK(!app_origin_.has_value() ||
        app_origin_->scheme() == chrome::kIsolatedAppScheme);
  profile_observation_.Observe(profile);
}

IsolatedWebAppURLLoaderFactory::~IsolatedWebAppURLLoaderFactory() = default;

void IsolatedWebAppURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> original_loader_client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(resource_request.url.SchemeIs(chrome::kIsolatedAppScheme));
  DCHECK(resource_request.url.IsStandard());

  auto header_injection_client =
      std::make_unique<HeaderInjectionURLLoaderClient>(
          std::move(original_loader_client));
  HeaderInjectionURLLoaderClient* header_injection_client_ptr =
      header_injection_client.get();
  mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client;
  mojo::MakeSelfOwnedReceiver(std::move(header_injection_client),
                              loader_client.InitWithNewPipeAndPassReceiver());

  if (!CanRequestUrl(resource_request.url)) {
    network::URLLoaderCompletionStatus status(net::ERR_BLOCKED_BY_CLIENT);
    mojo::Remote<network::mojom::URLLoaderClient>(std::move(loader_client))
        ->OnComplete(status);
    return;
  }

  auto* provider = WebAppProvider::GetForWebApps(profile_);
  if (!provider) {
    LogErrorAndFail("Web Apps are not available for this profile.",
                    std::move(loader_client));
    return;
  }
  if (!provider->on_registry_ready().is_signaled()) {
    provider->on_registry_ready().Post(
        FROM_HERE,
        base::BindOnce(&IsolatedWebAppURLLoaderFactory::CreateLoaderAndStart,
                       weak_factory_.GetWeakPtr(), std::move(loader_receiver),
                       request_id, options, resource_request,
                       std::move(loader_client), traffic_annotation));
    return;
  }

  ASSIGN_OR_RETURN(IsolatedWebAppUrlInfo url_info,
                   IsolatedWebAppUrlInfo::Create(resource_request.url),
                   [&](std::string error) {
                     LogErrorAndFail(std::move(error),
                                     std::move(loader_client));
                   });

  if (frame_tree_node_id_.has_value()) {
    auto* web_contents =
        content::WebContents::FromFrameTreeNodeId(*frame_tree_node_id_);
    if (web_contents == nullptr) {
      // `web_contents` can be `nullptr` in certain edge cases, such as when the
      // browser window closes concurrently with an ongoing request (see
      // crbug.com/1477761). Return an error if that is the case, instead of
      // silently not querying `IsolatedWebAppPendingInstallInfo`. Should we
      // ever find a case where we _do_ want to continue request processing even
      // though the `WebContents` no longer exists, we can change the below code
      // to skip checking `IsolatedWebAppPendingInstallInfo` instead of
      // returning an error.
      LogErrorAndFail("Unable to find WebContents based on frame tree node id.",
                      std::move(loader_client));
      return;
    }
    std::optional<IwaSourceWithMode> pending_install_app_source =
        IsolatedWebAppPendingInstallInfo::FromWebContents(*web_contents)
            .source();

    if (pending_install_app_source.has_value()) {
      HandleRequest(url_info, *pending_install_app_source,
                    /*is_pending_install=*/true, std::move(loader_receiver),
                    resource_request, std::move(loader_client),
                    traffic_annotation);
      return;
    }
  }

  ASSIGN_OR_RETURN(const WebApp& iwa, FindIsolatedWebApp(*provider, url_info),
                   [&](std::string error) {
                     LogErrorAndFail(std::move(error),
                                     std::move(loader_client));
                   });
  auto location = IwaSourceWithMode::FromStorageLocation(
      profile_->GetPath(), iwa.isolation_data()->location());

  if (iwa.isolation_data()->location().dev_mode() &&
      !IsIwaDevModeEnabled(&*profile_)) {
    LogErrorAndFail(base::StrCat({"Unable to load Isolated Web App that was "
                                  "installed in Developer Mode: ",
                                  kIwaDevModeNotEnabledMessage}),
                    std::move(loader_client));
    return;
  }

  std::optional<std::string> csp_override = ComputeCspOverride(location);
  if (csp_override.has_value()) {
    header_injection_client_ptr->set_csp_override(csp_override.value());
  }

  IsolatedWebAppUpdateManager& update_manager = provider->iwa_update_manager();
  auto pass_key = base::PassKey<IsolatedWebAppURLLoaderFactory>();
  if (update_manager.IsUpdateBeingApplied(pass_key, url_info.app_id())) {
    update_manager.PrioritizeUpdateAndWait(
        pass_key, url_info.app_id(),
        // We ignore whether or not the update was applied successfully - if it
        // succeeds, we send the request to the updated version. If it fails, we
        // send the request to the previous version and rely on the update
        // system to retry the update at a later point.
        base::IgnoreArgs<IsolatedWebAppUpdateApplyTask::CompletionStatus>(
            base::BindOnce(&IsolatedWebAppURLLoaderFactory::HandleRequest,
                           weak_factory_.GetWeakPtr(), url_info, location,
                           /*is_pending_install=*/false,
                           std::move(loader_receiver), resource_request,
                           std::move(loader_client), traffic_annotation)));
    return;
  }

  HandleRequest(url_info, location,
                /*is_pending_install=*/false, std::move(loader_receiver),
                resource_request, std::move(loader_client), traffic_annotation);
}

void IsolatedWebAppURLLoaderFactory::HandleRequest(
    const IsolatedWebAppUrlInfo& url_info,
    const IwaSourceWithMode& source,
    bool is_pending_install,
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (!IsSupportedHttpMethod(resource_request.method)) {
    CompleteWithGeneratedResponse(
        mojo::Remote<network::mojom::URLLoaderClient>(std::move(loader_client)),
        net::HTTP_METHOD_NOT_ALLOWED);
    return;
  }

  if (is_pending_install && resource_request.url.path() == kInstallPagePath) {
    CompleteWithGeneratedResponse(
        mojo::Remote<network::mojom::URLLoaderClient>(std::move(loader_client)),
        net::HTTP_OK, kInstallPageContent);
    return;
  }

  absl::visit(
      base::Overloaded{
          [&](const IwaSourceBundleWithMode& source) {
            CHECK(!url_info.web_bundle_id().is_for_proxy_mode());
            HandleSignedBundle(source.path(), source.dev_mode(),
                               url_info.web_bundle_id(),
                               std::move(loader_receiver), resource_request,
                               std::move(loader_client));
          },
          [&](const IwaSourceProxy& source) {
            CHECK(url_info.web_bundle_id().is_for_proxy_mode());
            HandleProxy(url_info, source, std::move(loader_receiver),
                        resource_request, std::move(loader_client),
                        traffic_annotation);
          }},
      source.variant());
}

void IsolatedWebAppURLLoaderFactory::OnProfileWillBeDestroyed(
    Profile* profile) {
  if (profile == profile_) {
    // When `profile_` gets destroyed, `this` factory is not able to serve any
    // more requests.
    profile_observation_.Reset();
    DisconnectReceiversAndDestroy();
  }
}

void IsolatedWebAppURLLoaderFactory::HandleSignedBundle(
    const base::FilePath& path,
    bool dev_mode,
    const web_package::SignedWebBundleId& web_bundle_id,
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client) {
  auto* isolated_web_app_reader_registry =
      IsolatedWebAppReaderRegistryFactory::GetForProfile(profile_);
  if (!isolated_web_app_reader_registry) {
    LogErrorAndFail("Support for Isolated Web Apps is not enabled.",
                    std::move(loader_client));
    return;
  }

  auto loader = std::make_unique<IsolatedWebAppURLLoader>(
      isolated_web_app_reader_registry, path, dev_mode, web_bundle_id,
      std::move(loader_client), resource_request, frame_tree_node_id_);
  mojo::MakeSelfOwnedReceiver(std::move(loader), std::move(loader_receiver));
}

namespace {

GURL ConstructProxyUrl(const IwaSourceProxy& proxy,
                       const GURL& resource_request_url) {
  GURL::Replacements replacements;
  replacements.SetPathStr(resource_request_url.path_piece());
  if (resource_request_url.has_query()) {
    replacements.SetQueryStr(resource_request_url.query_piece());
  }
  return proxy.proxy_url().GetURL().ReplaceComponents(replacements);
}

network::ResourceRequest ConstructProxyRequest(
    const IwaSourceProxy& proxy,
    const network::ResourceRequest& resource_request) {
  // Create a new ResourceRequest with the proxy URL.
  network::ResourceRequest proxy_request;
  proxy_request.url = ConstructProxyUrl(proxy, resource_request.url);
  proxy_request.method = net::HttpRequestHeaders::kGetMethod;
  // Don't send cookies or HTTP authentication to the proxy server.
  proxy_request.credentials_mode = network::mojom::CredentialsMode::kOmit;

  std::string accept_header_value =
      resource_request.headers.GetHeader(net::HttpRequestHeaders::kAccept)
          .value_or(network::kDefaultAcceptHeaderValue);
  proxy_request.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                  accept_header_value);
  proxy_request.headers.SetHeader(net::HttpRequestHeaders::kCacheControl,
                                  "no-cache");
  return proxy_request;
}

}  // namespace

void IsolatedWebAppURLLoaderFactory::HandleProxy(
    const IsolatedWebAppUrlInfo& url_info,
    const IwaSourceProxy& proxy,
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(!proxy.proxy_url().opaque());
  content::StoragePartition* storage_partition =
      profile_->GetStoragePartition(url_info.storage_partition_config(profile_),
                                    /*can_create=*/false);
  if (storage_partition == nullptr) {
    LogErrorAndFail("Storage not found for Isolated Web App: " +
                        resource_request.url.spec(),
                    std::move(loader_client));
    return;
  }

  storage_partition->GetURLLoaderFactoryForBrowserProcess()
      ->CreateLoaderAndStart(std::move(loader_receiver),
                             /*request_id=*/0,
                             network::mojom::kURLLoadOptionNone,
                             ConstructProxyRequest(proxy, resource_request),
                             std::move(loader_client), traffic_annotation);
}

void IsolatedWebAppURLLoaderFactory::LogErrorAndFail(
    const std::string& error_message,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  LogErrorMessageToConsole(frame_tree_node_id_, error_message);

  mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
      ->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
}

bool IsolatedWebAppURLLoaderFactory::CanRequestUrl(const GURL& url) const {
  // If no origin was specified we should allow the request. This will be the
  // case for navigations and worker script/update loads.
  if (!app_origin_) {
    return true;
  }
  return app_origin_->IsSameOriginWith(url);
}

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
IsolatedWebAppURLLoaderFactory::CreateForFrame(
    content::BrowserContext* browser_context,
    std::optional<url::Origin> app_origin,
    content::FrameTreeNodeId frame_tree_node_id) {
  return CreateInternal(browser_context, std::move(app_origin),
                        frame_tree_node_id);
}

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
IsolatedWebAppURLLoaderFactory::Create(content::BrowserContext* browser_context,
                                       std::optional<url::Origin> app_origin) {
  return CreateInternal(browser_context, std::move(app_origin),
                        /*frame_tree_node_id=*/std::nullopt);
}

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
IsolatedWebAppURLLoaderFactory::CreateInternal(
    content::BrowserContext* browser_context,
    std::optional<url::Origin> app_origin,
    std::optional<content::FrameTreeNodeId> frame_tree_node_id) {
  DCHECK(browser_context);
  DCHECK(!browser_context->ShutdownStarted());

  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;

  // The IsolatedWebAppURLLoaderFactory will delete itself when there are no
  // more receivers - see the
  // network::SelfDeletingURLLoaderFactory::OnDisconnect method.
  new IsolatedWebAppURLLoaderFactory(
      Profile::FromBrowserContext(browser_context), std::move(app_origin),
      frame_tree_node_id, pending_remote.InitWithNewPipeAndPassReceiver());

  return pending_remote;
}

}  // namespace web_app
