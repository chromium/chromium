// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_file.h"
#include "base/functional/overloaded.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_declaration.h"
#include "third_party/blink/public/common/permissions_policy/policy_helper_public.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {
namespace {

constexpr char kInstallPagePath[] = "/.well-known/_generated_install_page.html";
constexpr char kManifestPath[] = "/.well-known/manifest.webmanifest";

using InstallResult = base::expected<InstallIsolatedWebAppCommandSuccess,
                                     InstallIsolatedWebAppCommandError>;

base::FilePath GetTestDataRelativePath(std::string_view subpath) {
  base::FilePath data_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &data_dir);
  return data_dir.AppendASCII("chrome/test/data/").AppendASCII(subpath);
}

FakeWebAppProvider* GetFakeWebAppProvider(Profile* profile) {
  // We can't use FakeWebAppProvider::Get here because we don't want it to
  // CHECK that FakeWebAppProvider is non-null.
  return WebAppProvider::GetForWebApps(profile)
      ->AsFakeWebAppProviderForTesting();
}

void FakeInstallPageState(Profile* profile,
                          const IsolatedWebAppUrlInfo& url_info,
                          blink::mojom::ManifestPtr blink_manifest) {
  FakeWebAppProvider* fake_web_app_provider = GetFakeWebAppProvider(profile);
  CHECK(fake_web_app_provider) << "WebAppProvider isn't faked";
  auto& fake_web_contents_manager = static_cast<FakeWebContentsManager&>(
      fake_web_app_provider->web_contents_manager());

  GURL base_url = url_info.origin().GetURL();
  for (const blink::Manifest::ImageResource& icon : blink_manifest->icons) {
    FakeWebContentsManager::FakeIconState& icon_state =
        fake_web_contents_manager.GetOrCreateIconState(icon.src);
    // For now we use a placeholder icon rather than reading the icons
    // from the app.
    icon_state.bitmaps = {SkBitmap()};
    icon_state.bitmaps[0].allocN32Pixels(icon.sizes[0].width(),
                                         icon.sizes[0].height());
    icon_state.bitmaps[0].eraseColor(SK_ColorWHITE);
  }

  GURL install_url = base_url.Resolve(kInstallPagePath);
  FakeWebContentsManager::FakePageState& install_page_state =
      fake_web_contents_manager.GetOrCreatePageState(install_url);
  install_page_state.url_load_result =
      webapps::WebAppUrlLoaderResult::kUrlLoaded;
  install_page_state.error_code =
      webapps::InstallableStatusCode::NO_ERROR_DETECTED;
  install_page_state.manifest_url = base_url.Resolve(kManifestPath);
  install_page_state.valid_manifest_for_web_app = true;
  install_page_state.manifest_before_default_processing =
      std::move(blink_manifest);
}

base::expected<IsolatedWebAppUrlInfo, std::string> Install(
    Profile* profile,
    const web_package::SignedWebBundleId& web_bundle_id,
    const IsolatedWebAppInstallSource& install_source) {
  auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id);
  if (FakeWebAppProvider* fake_provider = GetFakeWebAppProvider(profile)) {
    if (!fake_provider->on_registry_ready().is_signaled() ||
        !fake_provider->on_external_managers_synchronized().is_signaled()) {
      LOG(WARNING)
          << "FakeWebAppProvider is not initialized. Try adding a call to "
          << "web_app::test::AwaitStartWebAppProviderAndSubsystems(Profile*) "
          << "during test setup.";
    }

    GURL install_url = url_info.origin().GetURL().Resolve(kInstallPagePath);
    auto& web_contents_manager = static_cast<FakeWebContentsManager&>(
        fake_provider->web_contents_manager());
    if (!web_contents_manager.HasPageState(install_url)) {
      LOG(WARNING) << "The install page for this IWA has not been faked. "
                   << "You likely need to call FakeInstallPageState before "
                   << "Install.";
    }
  }

  base::test::TestFuture<InstallResult> future;
  WebAppProvider::GetForWebApps(profile)->scheduler().InstallIsolatedWebApp(
      url_info, install_source,
      /*expected_version=*/std::nullopt,
      /*optional_keep_alive=*/nullptr,
      /*optional_profile_keep_alive=*/nullptr, future.GetCallback());
  CHECK(future.Wait());

  if (!future.Get().has_value()) {
    return base::unexpected(future.Get().error().message);
  }

  return url_info;
}

web_package::SignedWebBundleId CreateSignedWebBundleIdFromKeyPair(
    const web_package::test::KeyPair& key_pair) {
  return absl::visit(
      [](const auto& key_pair) {
        return web_package::SignedWebBundleId::CreateForPublicKey(
            key_pair.public_key);
      },
      key_pair);
}

}  // namespace

BundledIsolatedWebApp::BundledIsolatedWebApp(
    const web_package::SignedWebBundleId& web_bundle_id,
    const std::vector<uint8_t> serialized_bundle,
    const base::FilePath path,
    ManifestBuilder manifest_builder)
    : web_bundle_id_(web_bundle_id),
      path_(std::move(path)),
      manifest_builder_(manifest_builder) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  CHECK(base::WriteFile(path_, std::move(serialized_bundle)));
}

BundledIsolatedWebApp::~BundledIsolatedWebApp() = default;

void BundledIsolatedWebApp::TrustSigningKey() {
  AddTrustedWebBundleIdForTesting(web_bundle_id_);
}

std::string BundledIsolatedWebApp::GetBundleData() const {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string content;
  CHECK(base::ReadFileToString(path_, &content));
  return content;
}

IsolatedWebAppUrlInfo BundledIsolatedWebApp::InstallChecked(Profile* profile) {
  auto result = Install(profile);
  CHECK(result.has_value()) << result.error();
  return *result;
}

base::expected<IsolatedWebAppUrlInfo, std::string>
BundledIsolatedWebApp::Install(Profile* profile) {
  return ::web_app::Install(
      profile, web_bundle_id_,
      IsolatedWebAppInstallSource::FromGraphicalInstaller(
          web_app::IwaSourceBundleProdModeWithFileOp(
              path(), web_app::IwaSourceBundleProdFileOp::kCopy)));
}

base::expected<IsolatedWebAppUrlInfo, std::string>
BundledIsolatedWebApp::TrustBundleAndInstall(Profile* profile) {
  TrustSigningKey();
  return Install(profile);
}

void BundledIsolatedWebApp::FakeInstallPageState(Profile* profile) {
  auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_);
  ::web_app::FakeInstallPageState(
      profile, url_info, manifest_builder_.ToBlinkManifest(url_info.origin()));
}

// static
std::unique_ptr<ScopedBundledIsolatedWebApp>
ScopedBundledIsolatedWebApp::Create(
    const web_package::SignedWebBundleId& web_bundle_id,
    const std::vector<uint8_t> serialized_bundle,
    ManifestBuilder manifest_builder) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempFile bundle_file;
  CHECK(bundle_file.Create());

  return base::WrapUnique(new ScopedBundledIsolatedWebApp(
      web_bundle_id, std::move(serialized_bundle), std::move(bundle_file),
      std::move(manifest_builder)));
}

ScopedBundledIsolatedWebApp::ScopedBundledIsolatedWebApp(
    const web_package::SignedWebBundleId& web_bundle_id,
    const std::vector<uint8_t> serialized_bundle,
    base::ScopedTempFile bundle_file,
    ManifestBuilder manifest_builder)
    : BundledIsolatedWebApp(web_bundle_id,
                            std::move(serialized_bundle),
                            bundle_file.path(),
                            std::move(manifest_builder)),
      bundle_file_(std::move(bundle_file)) {}

ScopedBundledIsolatedWebApp::~ScopedBundledIsolatedWebApp() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  bundle_file_.Reset();
}

ScopedProxyIsolatedWebApp::ScopedProxyIsolatedWebApp(
    std::unique_ptr<net::EmbeddedTestServer> proxy_server,
    std::optional<ManifestBuilder> manifest_builder)
    : proxy_server_(std::move(proxy_server)),
      manifest_builder_(manifest_builder) {}

ScopedProxyIsolatedWebApp::~ScopedProxyIsolatedWebApp() = default;

IsolatedWebAppUrlInfo ScopedProxyIsolatedWebApp::InstallChecked(
    Profile* profile) {
  auto result = Install(profile);
  CHECK(result.has_value()) << result.error();
  return *result;
}

base::expected<IsolatedWebAppUrlInfo, std::string>
ScopedProxyIsolatedWebApp::Install(Profile* profile) {
  return Install(profile,
                 web_package::SignedWebBundleId::CreateRandomForProxyMode());
}

base::expected<IsolatedWebAppUrlInfo, std::string>
ScopedProxyIsolatedWebApp::Install(
    Profile* profile,
    const web_package::SignedWebBundleId& web_bundle_id) {
  return ::web_app::Install(profile, web_bundle_id,
                            IsolatedWebAppInstallSource::FromDevUi(
                                IwaSourceProxy(proxy_server_->GetOrigin())));
}

ManifestBuilder::PermissionsPolicy::PermissionsPolicy(
    bool wildcard,
    bool self,
    std::vector<url::Origin> origins)
    : wildcard(wildcard), self(self), origins(origins) {
  CHECK(!(wildcard && self));
  CHECK(!(wildcard && !origins.empty()));
}

ManifestBuilder::PermissionsPolicy::PermissionsPolicy(
    const ManifestBuilder::PermissionsPolicy&) = default;
ManifestBuilder::PermissionsPolicy::~PermissionsPolicy() = default;

ManifestBuilder::ManifestBuilder()
    : name_("Test App"), version_("0.0.1"), start_url_("/") {
  AddPermissionsPolicy(
      blink::mojom::PermissionsPolicyFeature::kCrossOriginIsolated,
      /*self=*/true, /*origins=*/{});
}

ManifestBuilder::ManifestBuilder(const ManifestBuilder&) = default;
ManifestBuilder::~ManifestBuilder() = default;

ManifestBuilder& ManifestBuilder::SetName(std::string_view name) {
  name_ = name;
  return *this;
}

ManifestBuilder& ManifestBuilder::SetVersion(std::string_view version) {
  version_ = version;
  return *this;
}

ManifestBuilder& ManifestBuilder::SetStartUrl(std::string_view start_url) {
  start_url_ = start_url;
  return *this;
}

ManifestBuilder& ManifestBuilder::AddIcon(std::string_view resource_path,
                                          gfx::Size size,
                                          std::string_view content_type) {
  icons_.emplace_back(std::string(resource_path), size,
                      std::string(content_type));
  return *this;
}

ManifestBuilder& ManifestBuilder::AddPermissionsPolicyWildcard(
    blink::mojom::PermissionsPolicyFeature feature) {
  permissions_policy_.insert_or_assign(
      feature,
      ManifestBuilder::PermissionsPolicy(/*wildcard=*/true, /*self=*/false,
                                         /*origins=*/{}));
  return *this;
}

ManifestBuilder& ManifestBuilder::AddPermissionsPolicy(
    blink::mojom::PermissionsPolicyFeature feature,
    bool self,
    std::vector<url::Origin> origins) {
  permissions_policy_.insert_or_assign(
      feature,
      ManifestBuilder::PermissionsPolicy(/*wildcard=*/false, self, origins));
  return *this;
}

ManifestBuilder& ManifestBuilder::AddProtocolHandler(std::string_view protocol,
                                                     std::string_view url) {
  protocol_handlers_.emplace_back(std::string(protocol), std::string(url));
  return *this;
}

ManifestBuilder& ManifestBuilder::AddFileHandler(
    std::string_view action,
    const FileHandlerAccept& accept) {
  file_handlers_[std::string(action)] = accept;
  return *this;
}

const std::string& ManifestBuilder::start_url() const {
  return start_url_;
}

const std::vector<ManifestBuilder::IconMetadata>& ManifestBuilder::icons()
    const {
  return icons_;
}

base::Version ManifestBuilder::version() const {
  base::Version version(version_);
  CHECK(version.IsValid());
  return version;
}

std::string ManifestBuilder::ToJson() const {
  auto json = base::Value::Dict()
                  .Set("name", name_)
                  .Set("version", version_)
                  .Set("id", "/")
                  .Set("scope", "/")
                  .Set("start_url", start_url_)
                  .Set("display", "standalone");

  base::Value::Dict policies;
  for (const auto& policy : permissions_policy_) {
    base::Value::List values;
    if (policy.second.wildcard) {
      values.Append("*");
    }
    if (policy.second.self) {
      values.Append("self");
    }
    for (const auto& origin : policy.second.origins) {
      values.Append(origin.Serialize());
    }
    std::string_view feature_name =
        blink::GetPermissionsPolicyFeatureToNameMap().at(policy.first);
    policies.Set(feature_name, std::move(values));
  }
  json.Set("permissions_policy", std::move(policies));

  base::Value::List icons;
  for (const auto& icon : icons_) {
    icons.Append(
        base::Value::Dict()
            .Set("src", icon.resource_path)
            .Set("sizes", base::StringPrintf("%dx%d", icon.size.width(),
                                             icon.size.height()))
            .Set("type", icon.content_type));
  }
  json.Set("icons", std::move(icons));

  base::Value::List protocol_handlers;
  for (const auto& protocol_handler : protocol_handlers_) {
    protocol_handlers.Append(base::Value::Dict()
                                 .Set("protocol", protocol_handler.first)
                                 .Set("url", protocol_handler.second));
  }
  json.Set("protocol_handlers", std::move(protocol_handlers));

  if (!file_handlers_.empty()) {
    base::Value::List file_handlers;
    for (const auto& handler_entry : file_handlers_) {
      base::Value::Dict accept;
      for (const auto& accept_entry : handler_entry.second) {
        base::Value::List extensions;
        for (const auto& extension : accept_entry.second) {
          extensions.Append(extension);
        }
        accept.Set(accept_entry.first, std::move(extensions));
      }
      file_handlers.Append(base::Value::Dict()
                               .Set("action", handler_entry.first)
                               .Set("accept", std::move(accept)));
    }
    json.Set("file_handlers", std::move(file_handlers));
  }

  return base::WriteJsonWithOptions(json, base::OPTIONS_PRETTY_PRINT).value();
}

blink::mojom::ManifestPtr ManifestBuilder::ToBlinkManifest(
    const url::Origin& app_origin) const {
  GURL base_url = app_origin.GetURL();
  auto manifest = blink::mojom::Manifest::New();
  manifest->name = base::UTF8ToUTF16(name_);
  manifest->version = base::UTF8ToUTF16(version_);
  manifest->id = base_url;
  manifest->scope = base_url;
  manifest->start_url = base_url.Resolve(start_url_);
  manifest->display = blink::mojom::DisplayMode::kStandalone;

  for (const auto& icon : icons_) {
    blink::Manifest::ImageResource blink_icon;
    blink_icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
    blink_icon.src = base_url.Resolve(icon.resource_path);
    blink_icon.type = base::UTF8ToUTF16(icon.content_type);
    blink_icon.sizes.push_back(icon.size);
    manifest->icons.push_back(blink_icon);
  }

  for (const auto& protocol_handler_pair : protocol_handlers_) {
    blink::mojom::ManifestProtocolHandlerPtr protocol_handler =
        blink::mojom::ManifestProtocolHandler::New(
            base::UTF8ToUTF16(protocol_handler_pair.first),
            GURL(protocol_handler_pair.second));
    manifest->protocol_handlers.push_back(std::move(protocol_handler));
  }

  for (const auto& policy : permissions_policy_) {
    blink::ParsedPermissionsPolicyDeclaration decl;
    decl.feature = policy.first;
    if (policy.second.wildcard) {
      decl.matches_all_origins = true;
    }
    if (policy.second.self) {
      decl.self_if_matches = url::Origin::Create(base_url);
    }
    for (const auto& origin : policy.second.origins) {
      decl.allowed_origins.push_back(
          blink::OriginWithPossibleWildcards::FromOrigin(origin).value());
    }
    manifest->permissions_policy.push_back(decl);
  }

  for (const auto& file_handler : file_handlers_) {
    base::flat_map<std::u16string, std::vector<std::u16string>> accept;
    for (const auto& accept_entry : file_handler.second) {
      std::vector<std::u16string>& extensions =
          accept[base::UTF8ToUTF16(accept_entry.first)];
      for (const auto& extension : accept_entry.second) {
        extensions.push_back(base::UTF8ToUTF16(extension));
      }
    }
    auto handler = blink::mojom::ManifestFileHandler::New();
    handler->action = GURL(file_handler.first);
    handler->accept = accept;
    manifest->file_handlers.push_back(std::move(handler));
  }

  return manifest;
}

IsolatedWebAppBuilder::Resource::Resource(
    net::HttpStatusCode status,
    const IsolatedWebAppBuilder::Headers& headers,
    const IsolatedWebAppBuilder::ResourceBody& body)
    : status_(status), headers_(headers), body_(body) {}

IsolatedWebAppBuilder::Resource::Resource(
    const IsolatedWebAppBuilder::Resource&) = default;
IsolatedWebAppBuilder::Resource::~Resource() = default;

scoped_refptr<net::HttpResponseHeaders>
IsolatedWebAppBuilder::Resource::headers(std::string_view resource_path) const {
  scoped_refptr<net::HttpResponseHeaders> http_headers;

  if (const base::FilePath* path = absl::get_if<base::FilePath>(&body_)) {
    base::FilePath headers_path(
        path->AddExtension(net::test_server::kMockHttpHeadersExtension));
    if (base::PathExists(headers_path)) {
      std::string raw_headers;
      CHECK(base::ReadFileToString(headers_path, &raw_headers));
      http_headers = net::HttpResponseHeaders::TryToCreate(raw_headers);
    }
  }

  if (!http_headers) {
    http_headers =
        net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200 OK")
            .Build();
  }

  bool has_content_type = false;
  for (const auto& header : headers_) {
    if (header.name == net::HttpRequestHeaders::kContentType) {
      has_content_type = true;
    }
    http_headers->AddHeader(header.name, header.value);
  }

  if (!has_content_type) {
    base::FilePath file_path =
        absl::visit(base::Overloaded{
                        [&](const std::string&) {
                          return base::FilePath::FromUTF8Unsafe(resource_path);
                        },
                        [&](const base::FilePath& path) { return path; },
                    },
                    body_);
    std::string content_type = net::test_server::GetContentType(file_path);
    if (content_type.empty()) {
      LOG(WARNING) << "Could not infer the Content-Type of " << file_path
                   << ". Falling back to application/octet-stream.";
      content_type = "application/octet-stream";
    }
    http_headers->AddHeader(net::HttpRequestHeaders::kContentType,
                            content_type);
  }
  return http_headers;
}

std::string IsolatedWebAppBuilder::Resource::body() const {
  return absl::visit(base::Overloaded{
                         [&](const std::string& content) { return content; },
                         [&](const base::FilePath& path) {
                           std::string content;
                           CHECK(base::ReadFileToString(path, &content));
                           return content;
                         },
                     },
                     body_);
}

IsolatedWebAppBuilder::IsolatedWebAppBuilder(
    const ManifestBuilder& manifest_builder)
    : manifest_builder_(manifest_builder) {
  AddHtml("/", "Test Isolated Web App");
  AddIconAsPng("/icon.png", CreateSquareIcon(256, SK_ColorBLUE));
}

IsolatedWebAppBuilder::IsolatedWebAppBuilder(const IsolatedWebAppBuilder&) =
    default;
IsolatedWebAppBuilder::~IsolatedWebAppBuilder() = default;

IsolatedWebAppBuilder& IsolatedWebAppBuilder::AddResource(
    std::string_view resource_path,
    std::string_view content,
    std::string_view content_type) {
  CHECK(net::HttpUtil::IsValidHeaderValue(content_type))
      << "Invalid Content-Type: \"" << content_type
      << "\". Did you swap the `content` and `content_type` parameters "
      << "to IsolatedWebAppBuilder::AddResource?";
  return AddResource(
      resource_path, content,
      {{net::HttpRequestHeaders::kContentType, std::string(content_type)}});
}

IsolatedWebAppBuilder& IsolatedWebAppBuilder::AddResource(
    std::string_view resource_path,
    std::string_view content,
    const Headers& headers,
    net::HttpStatusCode status) {
  CHECK(resource_path != kManifestPath)
      << "The manifest must be specified through the ManifestBuilder";
  resources_.insert_or_assign(std::string(resource_path),
                              Resource(status, headers, std::string(content)));
  return *this;
}

IsolatedWebAppBuilder& IsolatedWebAppBuilder::AddHtml(
    std::string_view resource_path,
    std::string_view content) {
  return AddResource(resource_path, content, "text/html");
}

IsolatedWebAppBuilder& IsolatedWebAppBuilder::AddJs(
    std::string_view resource_path,
    std::string_view content) {
  return AddResource(resource_path, content, "text/javascript");
}

IsolatedWebAppBuilder& IsolatedWebAppBuilder::AddIconAsPng(
    std::string_view resource_path,
    const SkBitmap& image) {
  manifest_builder_.AddIcon(
      resource_path, gfx::Size(image.width(), image.height()), "image/png");
  return AddImageAsPng(resource_path, image);
}

IsolatedWebAppBuilder& IsolatedWebAppBuilder::AddImageAsPng(
    std::string_view resource_path,
    const SkBitmap& image) {
  SkDynamicMemoryWStream stream;
  CHECK(SkPngEncoder::Encode(&stream, image.pixmap(), {}));
  sk_sp<SkData> icon_skdata = stream.detachAsData();
  std::string png(static_cast<const char*>(icon_skdata->data()),
                  icon_skdata->size());
  return AddResource(resource_path, png, "image/png");
}

IsolatedWebAppBuilder& IsolatedWebAppBuilder::AddFileFromDisk(
    std::string_view resource_path,
    const base::FilePath& file_path,
    const Headers& headers) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  CHECK(base::PathExists(file_path)) << file_path << " does not exist";
  resources_.insert_or_assign(std::string(resource_path),
                              Resource(net::HTTP_OK, headers, file_path));
  return *this;
}

IsolatedWebAppBuilder& IsolatedWebAppBuilder::AddFileFromDisk(
    std::string_view resource_path,
    std::string_view chrome_test_data_relative_path,
    const Headers& headers) {
  return AddFileFromDisk(
      resource_path, GetTestDataRelativePath(chrome_test_data_relative_path),
      headers);
}

IsolatedWebAppBuilder& IsolatedWebAppBuilder::AddFolderFromDisk(
    std::string_view resource_path,
    const base::FilePath& folder_path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  CHECK(base::PathExists(folder_path)) << folder_path << " does not exist";
  base::FileEnumerator files(folder_path, /*recursive=*/true,
                             base::FileEnumerator::FILES);
  for (base::FilePath path = files.Next(); !path.empty(); path = files.Next()) {
    base::FilePath relative(FILE_PATH_LITERAL("/"));
    CHECK(folder_path.AppendRelativePath(path, &relative));
    AddFileFromDisk(relative.AsUTF8Unsafe(), path);
  }
  return *this;
}

IsolatedWebAppBuilder& IsolatedWebAppBuilder::AddFolderFromDisk(
    std::string_view resource_path,
    std::string_view chrome_test_data_relative_path) {
  return AddFolderFromDisk(
      resource_path, GetTestDataRelativePath(chrome_test_data_relative_path));
}

IsolatedWebAppBuilder& IsolatedWebAppBuilder::RemoveResource(
    std::string_view resource_path) {
  resources_.erase(std::string(resource_path));
  return *this;
}

std::unique_ptr<ScopedProxyIsolatedWebApp>
IsolatedWebAppBuilder::BuildAndStartProxyServer() {
  Validate();
  net::EmbeddedTestServer::HandleRequestCallback handler = base::BindRepeating(
      &IsolatedWebAppBuilder::HandleRequest, manifest_builder_, resources_);
  auto server = std::make_unique<net::EmbeddedTestServer>();
  server->RegisterRequestHandler(handler);
  CHECK(server->Start());
  return std::make_unique<ScopedProxyIsolatedWebApp>(std::move(server),
                                                     manifest_builder_);
}

std::unique_ptr<ScopedBundledIsolatedWebApp>
IsolatedWebAppBuilder::BuildBundle() {
  return BuildBundle(web_package::test::Ed25519KeyPair::CreateRandom());
}

std::unique_ptr<ScopedBundledIsolatedWebApp> IsolatedWebAppBuilder::BuildBundle(
    const web_package::test::KeyPair& key_pair) {
  return BuildBundle(CreateSignedWebBundleIdFromKeyPair(key_pair), {key_pair});
}

std::unique_ptr<ScopedBundledIsolatedWebApp> IsolatedWebAppBuilder::BuildBundle(
    const web_package::SignedWebBundleId& web_bundle_id,
    const web_package::test::KeyPairs& key_pairs) {
  return ScopedBundledIsolatedWebApp::Create(
      web_bundle_id, BuildInMemoryBundle(web_bundle_id, key_pairs),
      manifest_builder_);
}

std::unique_ptr<BundledIsolatedWebApp> IsolatedWebAppBuilder::BuildBundle(
    const base::FilePath& bundle_path) {
  return BuildBundle(bundle_path,
                     web_package::test::Ed25519KeyPair::CreateRandom());
}

std::unique_ptr<BundledIsolatedWebApp> IsolatedWebAppBuilder::BuildBundle(
    const base::FilePath& bundle_path,
    const web_package::test::KeyPair& key_pair) {
  return BuildBundle(bundle_path, CreateSignedWebBundleIdFromKeyPair(key_pair),
                     {key_pair});
}

std::unique_ptr<BundledIsolatedWebApp> IsolatedWebAppBuilder::BuildBundle(
    const base::FilePath& bundle_path,
    const web_package::SignedWebBundleId& web_bundle_id,
    const web_package::test::KeyPairs& key_pairs) {
  return std::make_unique<BundledIsolatedWebApp>(
      web_bundle_id, BuildInMemoryBundle(web_bundle_id, key_pairs), bundle_path,
      manifest_builder_);
}

std::vector<uint8_t> IsolatedWebAppBuilder ::BuildInMemoryBundle(
    const web_package::SignedWebBundleId& web_bundle_id,
    const web_package::test::KeyPairs& key_pairs) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  Validate();
  web_package::WebBundleBuilder builder;
  for (const auto& resource : resources_) {
    scoped_refptr<net::HttpResponseHeaders> headers =
        resource.second.headers(resource.first);

    web_package::WebBundleBuilder::Headers bundle_headers = {
        {":status", base::ToString(resource.second.status())}};
    size_t iterator = 0;
    std::string name;
    std::string value;
    while (headers->EnumerateHeaderLines(&iterator, &name, &value)) {
      // Web Bundle header names must be lowercase.
      // See section 8.1.2 of [RFC7540].
      bundle_headers.push_back({base::ToLowerASCII(name), value});
    }

    builder.AddExchange(resource.first, bundle_headers, resource.second.body());
  }

  builder.AddExchange(
      kManifestPath,
      {{":status", "200"}, {"content-type", "application/manifest+json"}},
      manifest_builder_.ToJson());

  return web_package::test::WebBundleSigner::SignBundle(
      builder.CreateBundle(), key_pairs,
      {{.web_bundle_id = web_bundle_id.id()}});
}

void IsolatedWebAppBuilder::Validate() {
  if (resources_.find(manifest_builder_.start_url()) == resources_.end()) {
    LOG(WARNING) << "Resource at 'start_url' (" << manifest_builder_.start_url()
                 << ") does not exist";
  }

  for (const auto& icon : manifest_builder_.icons()) {
    if (resources_.find(icon.resource_path) == resources_.end()) {
      LOG(WARNING) << "Icon at '" << icon.resource_path << "' does not exist";
    }
  }
}

// static
std::unique_ptr<net::test_server::HttpResponse>
IsolatedWebAppBuilder::HandleRequest(
    const ManifestBuilder& manifest_builder,
    const std::map<std::string, Resource>& resources,
    const net::test_server::HttpRequest& request) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  std::string path = request.GetURL().path();
  if (path == kManifestPath) {
    response->set_code(net::HTTP_OK);
    response->set_content_type("application/manifest+json");
    response->set_content(manifest_builder.ToJson());
  } else if (const auto resource = resources.find(path);
             resource != resources.end()) {
    response->set_code(resource->second.status());
    response->set_content(resource->second.body());

    scoped_refptr<net::HttpResponseHeaders> headers =
        resource->second.headers(resource->first);

    size_t iterator = 0;
    std::string name;
    std::string value;
    while (headers->EnumerateHeaderLines(&iterator, &name, &value)) {
      if (name == net::HttpRequestHeaders::kContentType) {
        response->set_content_type(value);
      } else {
        response->AddCustomHeader(name, value);
      }
    }
  } else {
    response->set_code(net::HTTP_NOT_FOUND);
  }
  return response;
}

}  // namespace web_app
