// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/devtools_ui.h"

#include <list>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "chrome/browser/devtools/url_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/user_agent.h"
#include "net/base/filename_util.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/blink/public/public_buildflags.h"

using content::BrowserThread;
using content::WebContents;

namespace {

std::string PathWithoutParams(const std::string& path) {
  return GURL(std::string("chrome-devtools://devtools/") + path)
      .path().substr(1);
}

scoped_refptr<base::RefCountedMemory> CreateNotFoundResponse() {
  const char kHttpNotFound[] = "HTTP/1.1 404 Not Found\n\n";
  return base::MakeRefCounted<base::RefCountedStaticMemory>(
      kHttpNotFound, strlen(kHttpNotFound));
}

// DevToolsDataSource ---------------------------------------------------------

std::string GetMimeTypeForPath(const std::string& path) {
  std::string filename = PathWithoutParams(path);
  if (base::EndsWith(filename, ".html", base::CompareCase::INSENSITIVE_ASCII)) {
    return "text/html";
  } else if (base::EndsWith(filename, ".css",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "text/css";
  } else if (base::EndsWith(filename, ".js",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "application/javascript";
  } else if (base::EndsWith(filename, ".png",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "image/png";
  } else if (base::EndsWith(filename, ".gif",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "image/gif";
  } else if (base::EndsWith(filename, ".svg",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "image/svg+xml";
  } else if (base::EndsWith(filename, ".manifest",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "text/cache-manifest";
  }
  return "text/html";
}

// An URLDataSource implementation that handles chrome-devtools://devtools/
// requests. Three types of requests could be handled based on the URL path:
// 1. /bundled/: bundled DevTools frontend is served.
// 2. /remote/: remote DevTools frontend is served from App Engine.
// 3. /custom/: custom DevTools frontend is served from the server as specified
//    by the --custom-devtools-frontend flag.
class DevToolsDataSource : public content::URLDataSource {
 public:
  using GotDataCallback = content::URLDataSource::GotDataCallback;

  explicit DevToolsDataSource(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : url_loader_factory_(std::move(url_loader_factory)) {}
  ~DevToolsDataSource() override = default;

  // content::URLDataSource implementation.
  std::string GetSource() const override;

  void StartDataRequest(
      const std::string& path,
      const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
      const GotDataCallback& callback) override;

 private:
  struct PendingRequest;

  // content::URLDataSource overrides.
  std::string GetMimeType(const std::string& path) const override;
  bool ShouldAddContentSecurityPolicy() const override;
  bool ShouldDenyXFrameOptions() const override;
  bool ShouldServeMimeTypeAsContentTypeHeader() const override;

  void OnLoadComplete(std::list<PendingRequest>::iterator request_iter,
                      std::unique_ptr<std::string> response_body);

  // Serves bundled DevTools frontend from ResourceBundle.
  void StartBundledDataRequest(const std::string& path,
                               const GotDataCallback& callback);

  // Serves remote DevTools frontend from hard-coded App Engine domain.
  void StartRemoteDataRequest(const GURL& url, const GotDataCallback& callback);

  // Serves remote DevTools frontend from any endpoint, passed through
  // command-line flag.
  void StartCustomDataRequest(const GURL& url,
                              const GotDataCallback& callback);

  void StartNetworkRequest(
      const GURL& url,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      int load_flags,
      const GotDataCallback& callback);

#if BUILDFLAG(DEBUG_DEVTOOLS)
  void StartFileRequestForDebugDevtools(const std::string& path,
                                        const GotDataCallback& callback);
#endif

  struct PendingRequest {
    PendingRequest() = default;
    PendingRequest(PendingRequest&& other) = default;
    PendingRequest& operator=(PendingRequest&& other) = default;

    ~PendingRequest() {
      if (callback)
        callback.Run(CreateNotFoundResponse());
    }

    GotDataCallback callback;
    std::unique_ptr<network::SimpleURLLoader> loader;

    DISALLOW_COPY_AND_ASSIGN(PendingRequest);
  };

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::list<PendingRequest> pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsDataSource);
};

std::string DevToolsDataSource::GetSource() const {
  return chrome::kChromeUIDevToolsHost;
}

void DevToolsDataSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  // Serve request from local bundle.
  std::string bundled_path_prefix(chrome::kChromeUIDevToolsBundledPath);
  bundled_path_prefix += "/";
  if (base::StartsWith(path, bundled_path_prefix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    std::string path_without_params = PathWithoutParams(path);

    DCHECK(base::StartsWith(path_without_params, bundled_path_prefix,
                            base::CompareCase::INSENSITIVE_ASCII));
    std::string path_under_bundled =
        path_without_params.substr(bundled_path_prefix.length());
#if BUILDFLAG(DEBUG_DEVTOOLS)
    StartFileRequestForDebugDevtools(path_under_bundled, callback);
#else
    StartBundledDataRequest(path_under_bundled, callback);
#endif
    return;
  }

  // Serve empty page.
  std::string empty_path_prefix(chrome::kChromeUIDevToolsBlankPath);
  if (base::StartsWith(path, empty_path_prefix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    callback.Run(new base::RefCountedStaticMemory());
    return;
  }

  // Serve request from remote location.
  std::string remote_path_prefix(chrome::kChromeUIDevToolsRemotePath);
  remote_path_prefix += "/";
  if (base::StartsWith(path, remote_path_prefix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    GURL url(kRemoteFrontendBase + path.substr(remote_path_prefix.length()));

    CHECK_EQ(url.host(), kRemoteFrontendDomain);
    if (url.is_valid() && DevToolsUIBindings::IsValidRemoteFrontendURL(url)) {
      StartRemoteDataRequest(url, callback);
    } else {
      DLOG(ERROR) << "Refusing to load invalid remote front-end URL";
      callback.Run(CreateNotFoundResponse());
    }
    return;
  }

  std::string custom_frontend_url =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kCustomDevtoolsFrontend);

  if (custom_frontend_url.empty()) {
    callback.Run(NULL);
    return;
  }

  // Serve request from custom location.
  std::string custom_path_prefix(chrome::kChromeUIDevToolsCustomPath);
  custom_path_prefix += "/";

  if (base::StartsWith(path, custom_path_prefix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    GURL url = GURL(custom_frontend_url +
                    path.substr(custom_path_prefix.length()));
    DCHECK(url.is_valid());
    StartCustomDataRequest(url, callback);
    return;
  }

  callback.Run(NULL);
}

std::string DevToolsDataSource::GetMimeType(const std::string& path) const {
  return GetMimeTypeForPath(path);
}

bool DevToolsDataSource::ShouldAddContentSecurityPolicy() const {
  return false;
}

bool DevToolsDataSource::ShouldDenyXFrameOptions() const {
  return false;
}

bool DevToolsDataSource::ShouldServeMimeTypeAsContentTypeHeader() const {
  return true;
}

void DevToolsDataSource::StartBundledDataRequest(
    const std::string& path,
    const content::URLDataSource::GotDataCallback& callback) {
  base::StringPiece resource =
      content::DevToolsFrontendHost::GetFrontendResource(path);

  DLOG_IF(WARNING, resource.empty())
      << "Unable to find dev tool resource: " << path
      << ". If you compiled with debug_devtools=1, try running with "
         "--debug-devtools.";
  scoped_refptr<base::RefCountedStaticMemory> bytes(
      new base::RefCountedStaticMemory(resource.data(), resource.length()));
  callback.Run(bytes.get());
}

void DevToolsDataSource::StartRemoteDataRequest(
    const GURL& url,
    const content::URLDataSource::GotDataCallback& callback) {
  CHECK(url.is_valid());
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("devtools_hard_coded_data_source", R"(
        semantics {
          sender: "Developer Tools Remote Data Request From Google"
          description:
            "This service fetches Chromium DevTools front-end files from the "
            "cloud for the remote debugging scenario."
          trigger:
            "When user attaches to mobile phone for debugging."
          data: "None"
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "This feature cannot be disabled by settings."
          chrome_policy {
            DeveloperToolsAvailability {
              policy_options {mode: MANDATORY}
              DeveloperToolsAvailability: 2
            }
          }
        })");

  StartNetworkRequest(url, traffic_annotation, net::LOAD_NORMAL, callback);
}

void DevToolsDataSource::StartCustomDataRequest(
    const GURL& url,
    const content::URLDataSource::GotDataCallback& callback) {
  if (!url.is_valid()) {
    callback.Run(CreateNotFoundResponse());
    return;
  }
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("devtools_free_data_source", R"(
        semantics {
          sender: "Developer Tools Remote Data Request"
          description:
            "This service fetches Chromium DevTools front-end files from the "
            "cloud for the remote debugging scenario. This can only happen if "
            "a URL was passed on the commandline via flag "
            "'--custom-devtools-frontend'. This URL overrides the default "
            "fetching from a Google website, see "
            "devtools_hard_coded_data_source."
          trigger:
            "When command line flag --custom-devtools-frontend is specified "
            "and DevTools is opened."
          data: "None"
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "This feature cannot be disabled by settings."
          chrome_policy {
            DeveloperToolsAvailability {
              policy_options {mode: MANDATORY}
              DeveloperToolsAvailability: 2
            }
          }
        })");

  StartNetworkRequest(url, traffic_annotation, net::LOAD_DISABLE_CACHE,
                      callback);
}

void DevToolsDataSource::StartNetworkRequest(
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    int load_flags,
    const GotDataCallback& callback) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->load_flags = load_flags;

  auto request_iter = pending_requests_.emplace(pending_requests_.begin());
  request_iter->callback = callback;
  request_iter->loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  request_iter->loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&DevToolsDataSource::OnLoadComplete,
                     base::Unretained(this), request_iter));
}

#if BUILDFLAG(DEBUG_DEVTOOLS)
scoped_refptr<base::RefCountedMemory> ReadFile(const base::FilePath& path) {
  std::string buffer;
  if (!base::ReadFileToString(path, &buffer)) {
    LOG(ERROR) << "Failed to read " << path;
    return CreateNotFoundResponse();
  }
  return base::RefCountedString::TakeString(&buffer);
}

void DevToolsDataSource::StartFileRequestForDebugDevtools(
    const std::string& path,
    const GotDataCallback& callback) {
  base::FilePath inspector_debug_dir;
  if (!base::PathService::Get(chrome::DIR_INSPECTOR_DEBUG,
                              &inspector_debug_dir)) {
    callback.Run(CreateNotFoundResponse());
    return;
  }

  DCHECK(!inspector_debug_dir.empty());

  base::FilePath full_path = inspector_debug_dir.AppendASCII(path);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::USER_VISIBLE},
      // The usage of BindRepeating below is only because the type of
      // task callback needs to match that of response callback, which
      // is currently a repeating callback.
      base::BindRepeating(ReadFile, std::move(full_path)), callback);
}

#endif  // BUILDFLAG(DEBUG_DEVTOOLS)

void DevToolsDataSource::OnLoadComplete(
    std::list<PendingRequest>::iterator request_iter,
    std::unique_ptr<std::string> response_body) {
  std::move(request_iter->callback)
      .Run(response_body
               ? base::RefCountedString::TakeString(response_body.get())
               : CreateNotFoundResponse());
  pending_requests_.erase(request_iter);
}

}  // namespace

// DevToolsUI -----------------------------------------------------------------

// static
GURL DevToolsUI::GetProxyURL(const std::string& frontend_url) {
  GURL url(frontend_url);
  if (url.scheme() == content::kChromeDevToolsScheme &&
      url.host() == chrome::kChromeUIDevToolsHost)
    return GURL();
  if (!url.is_valid() || url.host() != kRemoteFrontendDomain)
    return GURL();
  return GURL(base::StringPrintf(
      "%s://%s/%s/%s?%s", content::kChromeDevToolsScheme,
      chrome::kChromeUIDevToolsHost, chrome::kChromeUIDevToolsRemotePath,
      url.path().substr(1).c_str(), url.query().c_str()));
}

// static
GURL DevToolsUI::GetRemoteBaseURL() {
  return GURL(base::StringPrintf(
      "%s%s/%s/",
      kRemoteFrontendBase,
      kRemoteFrontendPath,
      content::GetWebKitRevision().c_str()));
}

// static
bool DevToolsUI::IsFrontendResourceURL(const GURL& url) {
  if (url.host_piece() == kRemoteFrontendDomain)
    return true;

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kCustomDevtoolsFrontend)) {
    GURL custom_frontend_url =
        GURL(cmd_line->GetSwitchValueASCII(switches::kCustomDevtoolsFrontend));
    if (custom_frontend_url.is_valid() &&
        custom_frontend_url.scheme_piece() == url.scheme_piece() &&
        custom_frontend_url.host_piece() == url.host_piece() &&
        custom_frontend_url.EffectiveIntPort() == url.EffectiveIntPort() &&
        base::StartsWith(url.path_piece(), custom_frontend_url.path_piece(),
                         base::CompareCase::SENSITIVE)) {
      return true;
    }
  }
  return false;
}

DevToolsUI::DevToolsUI(content::WebUI* web_ui)
    : WebUIController(web_ui), bindings_(web_ui->GetWebContents()) {
  web_ui->SetBindings(0);
  auto factory = content::BrowserContext::GetDefaultStoragePartition(
                     web_ui->GetWebContents()->GetBrowserContext())
                     ->GetURLLoaderFactoryForBrowserProcess();
  content::URLDataSource::Add(
      web_ui->GetWebContents()->GetBrowserContext(),
      std::make_unique<DevToolsDataSource>(std::move(factory)));
}

DevToolsUI::~DevToolsUI() = default;
