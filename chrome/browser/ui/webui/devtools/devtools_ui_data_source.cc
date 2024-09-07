// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/devtools/devtools_ui_data_source.h"

#include <list>
#include <utility>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/devtools/devtools_ui_bindings.h"
#include "chrome/browser/devtools/url_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/base/filename_util.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/url_constants.h"

namespace {

std::string PathWithoutParams(const std::string& path) {
  return GURL(base::StrCat({content::kChromeDevToolsScheme,
                            url::kStandardSchemeSeparator,
                            chrome::kChromeUIDevToolsHost}))
      .Resolve(path)
      .path()
      .substr(1);
}

scoped_refptr<base::RefCountedMemory> CreateNotFoundResponse() {
  const char kHttpNotFound[] = "HTTP/1.1 404 Not Found\n\n";
  return base::MakeRefCounted<base::RefCountedStaticMemory>(
      base::byte_span_from_cstring(kHttpNotFound));
}

// DevToolsDataSource ---------------------------------------------------------

std::string GetMimeTypeForUrl(const GURL& url) {
  std::string filename = url.ExtractFileName();
  if (base::EndsWith(filename, ".html", base::CompareCase::INSENSITIVE_ASCII)) {
    return "text/html";
  } else if (base::EndsWith(filename, ".css",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "text/css";
  } else if (base::EndsWith(filename, ".js",
                            base::CompareCase::INSENSITIVE_ASCII) ||
             base::EndsWith(filename, ".mjs",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "application/javascript";
  } else if (base::EndsWith(filename, ".png",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "image/png";
  } else if (base::EndsWith(filename, ".map",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "application/json";
  } else if (base::EndsWith(filename, ".ts",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return "application/x-typescript";
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

// Checks if the path starts with a prefix followed by a revision. In that case,
// the prefix and the revision is removed from the path. For example,
// "$prefix@76e4c1bb2ab4671b8beba3444e61c0f17584b2fc/inspector.html" becomes
// "inspector.html".
std::string StripDevToolsRevisionWithPrefix(const std::string& path,
                                            const std::string& prefix) {
  if (base::StartsWith(path, prefix, base::CompareCase::INSENSITIVE_ASCII)) {
    std::size_t found = path.find("/", prefix.length() + 1);
    if (found != std::string::npos) {
      return path.substr(found + 1);
    }
    DLOG(ERROR) << "Unexpected URL format, falling back to the original URL.";
  }
  return path;
}
}  // namespace

DevToolsDataSource::DevToolsDataSource(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

DevToolsDataSource::~DevToolsDataSource() {}

std::string DevToolsDataSource::GetSource() {
  return chrome::kChromeUIDevToolsHost;
}

// static
GURL GetCustomDevToolsFrontendURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kCustomDevtoolsFrontend)) {
    return GURL(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kCustomDevtoolsFrontend));
  }
  return GURL();
}

bool DevToolsDataSource::MaybeHandleCustomRequest(const std::string& path,
                                                  GotDataCallback* callback) {
  GURL custom_devtools_frontend = GetCustomDevToolsFrontendURL();
  if (!custom_devtools_frontend.is_valid())
    return false;
  std::string stripped_path =
      StripDevToolsRevisionWithPrefix(path, "serve_rev/");
  stripped_path = StripDevToolsRevisionWithPrefix(stripped_path, "serve_file/");
  stripped_path =
      StripDevToolsRevisionWithPrefix(stripped_path, "serve_internal_file/");
  if (custom_devtools_frontend.SchemeIsFile()) {
    // Fetch from file system but strip all the params.
    StartFileRequest(PathWithoutParams(stripped_path), std::move(*callback));
    return true;
  }
  GURL remote_url(custom_devtools_frontend.spec() + stripped_path);
  // Fetch from remote URL.
  StartCustomDataRequest(remote_url, std::move(*callback));
  return true;
}

void DevToolsDataSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    GotDataCallback callback) {
  // Serve request to devtools://bundled/ from local bundle.
  // TODO(crbug.com/40050262): Simplify usages of |path| since |url| is
  // available.
  const std::string path = content::URLDataSource::URLToRequestPath(url);
  std::string bundled_path_prefix(chrome::kChromeUIDevToolsBundledPath);
  bundled_path_prefix += "/";
  if (base::StartsWith(path, bundled_path_prefix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    std::string path_without_params = PathWithoutParams(path);

    DCHECK(base::StartsWith(path_without_params, bundled_path_prefix,
                            base::CompareCase::INSENSITIVE_ASCII));
    std::string path_under_bundled =
        path_without_params.substr(bundled_path_prefix.length());
    if (!MaybeHandleCustomRequest(path_under_bundled, &callback)) {
      // Fetch from packaged resources.
      StartBundledDataRequest(path_under_bundled, std::move(callback));
    }
    return;
  }

  // Serve request to devtools://blank as empty page.
  std::string empty_path_prefix(chrome::kChromeUIDevToolsBlankPath);
  if (base::StartsWith(path, empty_path_prefix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    std::move(callback).Run(new base::RefCountedStaticMemory());
    return;
  }

  // Serve request to devtools://remote from remote location.
  std::string remote_path_prefix(chrome::kChromeUIDevToolsRemotePath);
  remote_path_prefix += "/";
  if (base::StartsWith(path, remote_path_prefix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    if (MaybeHandleCustomRequest(path.substr(remote_path_prefix.length()),
                                 &callback)) {
      return;
    }
    GURL remote_url(kRemoteFrontendBase +
                    path.substr(remote_path_prefix.length()));

    CHECK_EQ(remote_url.host(), kRemoteFrontendDomain);
    if (remote_url.is_valid() &&
        DevToolsUIBindings::IsValidRemoteFrontendURL(remote_url)) {
      StartRemoteDataRequest(remote_url, std::move(callback));
    } else {
      DLOG(ERROR) << "Refusing to load invalid remote front-end URL";
      std::move(callback).Run(CreateNotFoundResponse());
    }
    return;
  }

  // Serve request to devtools://custom from custom URL.
  std::string custom_path_prefix(chrome::kChromeUIDevToolsCustomPath);
  custom_path_prefix += "/";
  if (base::StartsWith(path, custom_path_prefix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    GURL custom_devtools_frontend = GetCustomDevToolsFrontendURL();
    if (!custom_devtools_frontend.is_empty()) {
      GURL devtools_url(custom_devtools_frontend.spec() +
                        path.substr(custom_path_prefix.length()));
      DCHECK(devtools_url.is_valid());
      StartCustomDataRequest(devtools_url, std::move(callback));
      return;
    }
  }

  std::move(callback).Run(CreateNotFoundResponse());
}

std::string DevToolsDataSource::GetMimeType(const GURL& url) {
  return GetMimeTypeForUrl(url);
}

bool DevToolsDataSource::ShouldAddContentSecurityPolicy() {
  return false;
}

bool DevToolsDataSource::ShouldDenyXFrameOptions() {
  return false;
}

bool DevToolsDataSource::ShouldServeMimeTypeAsContentTypeHeader() {
  return true;
}

void DevToolsDataSource::StartBundledDataRequest(
    const std::string& path,
    content::URLDataSource::GotDataCallback callback) {
  scoped_refptr<base::RefCountedMemory> bytes =
      content::DevToolsFrontendHost::GetFrontendResourceBytes(path);

  DLOG_IF(WARNING, !bytes) << "Unable to find DevTools resource: " << path;
  std::move(callback).Run(bytes);
}

void DevToolsDataSource::StartRemoteDataRequest(
    const GURL& url,
    content::URLDataSource::GotDataCallback callback) {
  CHECK(url.is_valid());
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("devtools_hard_coded_data_source",
                                          R"(
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
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          chrome_policy {
            DeveloperToolsAvailability {
              policy_options {mode: MANDATORY}
              DeveloperToolsAvailability: 2
            }
          }
        })");

  StartNetworkRequest(url, traffic_annotation, net::LOAD_NORMAL,
                      std::move(callback));
}

void DevToolsDataSource::StartCustomDataRequest(
    const GURL& url,
    content::URLDataSource::GotDataCallback callback) {
  if (!url.is_valid()) {
    std::move(callback).Run(CreateNotFoundResponse());
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
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          chrome_policy {
            DeveloperToolsAvailability {
              policy_options {mode: MANDATORY}
              DeveloperToolsAvailability: 2
            }
          }
        })");

  StartNetworkRequest(url, traffic_annotation, net::LOAD_DISABLE_CACHE,
                      std::move(callback));
}

void DevToolsDataSource::StartNetworkRequest(
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    int load_flags,
    GotDataCallback callback) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->load_flags = load_flags;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  auto request_iter = pending_requests_.emplace(pending_requests_.begin());
  request_iter->callback = std::move(callback);
  request_iter->loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  request_iter->loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&DevToolsDataSource::OnLoadComplete,
                     base::Unretained(this), request_iter));
}

scoped_refptr<base::RefCountedMemory> ReadFileForDevTools(
    const base::FilePath& path) {
  std::string buffer;
  if (!base::ReadFileToString(path, &buffer)) {
    LOG(ERROR) << "Failed to read " << path;
    return CreateNotFoundResponse();
  }
  return base::MakeRefCounted<base::RefCountedString>(std::move(buffer));
}

void DevToolsDataSource::StartFileRequest(const std::string& path,
                                          GotDataCallback callback) {
  base::FilePath base_path;
  GURL custom_devtools_frontend = GetCustomDevToolsFrontendURL();
  DCHECK(custom_devtools_frontend.SchemeIsFile());
  if (!net::FileURLToFilePath(custom_devtools_frontend, &base_path)) {
    std::move(callback).Run(CreateNotFoundResponse());
    LOG(WARNING) << "Unable to find DevTools resource: " << path;
    return;
  }

  base::FilePath full_path = base_path.AppendASCII(path);
  CHECK(base_path.IsParent(full_path));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::USER_VISIBLE},
      base::BindOnce(ReadFileForDevTools, std::move(full_path)),
      std::move(callback));
}

void DevToolsDataSource::OnLoadComplete(
    std::list<PendingRequest>::iterator request_iter,
    std::unique_ptr<std::string> response_body) {
  GotDataCallback callback = std::move(request_iter->callback);
  pending_requests_.erase(request_iter);
  std::move(callback).Run(response_body
                              ? base::MakeRefCounted<base::RefCountedString>(
                                    std::move(*response_body))
                              : CreateNotFoundResponse());
  // `this` might no longer be valid after `callback` has run.
}

DevToolsDataSource::PendingRequest::PendingRequest() = default;

DevToolsDataSource::PendingRequest::PendingRequest(PendingRequest&& other) =
    default;

DevToolsDataSource::PendingRequest::~PendingRequest() {
  if (callback)
    std::move(callback).Run(CreateNotFoundResponse());
}
