// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_file.h"
#include "base/functional/overloaded.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
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
constexpr char kManifestPath[] = "/manifest.webmanifest";

using InstallResult = base::expected<InstallIsolatedWebAppCommandSuccess,
                                     InstallIsolatedWebAppCommandError>;

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
    // For now we use a placeholder square icon rather than reading the icons
    // from the app.
    icon_state.bitmaps = {CreateSquareIcon(256, SK_ColorWHITE)};
  }

  GURL install_url = base_url.Resolve(kInstallPagePath);
  FakeWebContentsManager::FakePageState& install_page_state =
      fake_web_contents_manager.GetOrCreatePageState(install_url);
  install_page_state.url_load_result = WebAppUrlLoaderResult::kUrlLoaded;
  install_page_state.error_code =
      webapps::InstallableStatusCode::NO_ERROR_DETECTED;
  install_page_state.manifest_url = base_url.Resolve(kManifestPath);
  install_page_state.valid_manifest_for_web_app = true;
  install_page_state.opt_manifest = std::move(blink_manifest);
}

base::expected<IsolatedWebAppUrlInfo, std::string> Install(
    Profile* profile,
    const web_package::SignedWebBundleId& web_bundle_id,
    const IsolatedWebAppLocation& location) {
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
      url_info, location,
      /*expected_version=*/std::nullopt,
      /*optional_keep_alive=*/nullptr,
      /*optional_profile_keep_alive=*/nullptr, future.GetCallback());
  CHECK(future.Wait());

  if (!future.Get().has_value()) {
    return base::unexpected(future.Get().error().message);
  }

  return url_info;
}

}  // namespace

ScopedBundledIsolatedWebApp::ScopedBundledIsolatedWebApp(
    const web_package::SignedWebBundleId& web_bundle_id,
    const std::vector<uint8_t> serialized_bundle,
    std::optional<ManifestBuilder> manifest_builder)
    : web_bundle_id_(web_bundle_id), manifest_builder_(manifest_builder) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  CHECK(bundle_file_.Create());
  CHECK(base::WriteFile(bundle_file_.path(), serialized_bundle));
}

ScopedBundledIsolatedWebApp::~ScopedBundledIsolatedWebApp() = default;

void ScopedBundledIsolatedWebApp::TrustSigningKey() {
  AddTrustedWebBundleIdForTesting(web_bundle_id_);
}

IsolatedWebAppUrlInfo ScopedBundledIsolatedWebApp::InstallChecked(
    Profile* profile) {
  auto result = Install(profile);
  CHECK(result.has_value()) << result.error();
  return *result;
}

base::expected<IsolatedWebAppUrlInfo, std::string>
ScopedBundledIsolatedWebApp::Install(Profile* profile) {
  return ::web_app::Install(profile, web_bundle_id_,
                            InstalledBundle{.path = path()});
}

void ScopedBundledIsolatedWebApp::FakeInstallPageState(Profile* profile) {
  CHECK(manifest_builder_.has_value());
  auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_);
  ::web_app::FakeInstallPageState(
      profile, url_info, manifest_builder_->ToBlinkManifest(url_info.origin()));
}

ScopedProxyIsolatedWebApp::ScopedProxyIsolatedWebApp(
    std::unique_ptr<net::EmbeddedTestServer> proxy_server,
    std::optional<ManifestBuilder> manifest_builder)
    : proxy_server_(std::move(proxy_server)),
      manifest_builder_(manifest_builder) {}

ScopedProxyIsolatedWebApp::~ScopedProxyIsolatedWebApp() = default;

void ScopedProxyIsolatedWebApp::FakeInstallPageState(
    Profile* profile,
    const web_package::SignedWebBundleId& web_bundle_id) {
  CHECK(manifest_builder_.has_value());
  auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id);
  ::web_app::FakeInstallPageState(
      profile, url_info, manifest_builder_->ToBlinkManifest(url_info.origin()));
}

IsolatedWebAppUrlInfo ScopedProxyIsolatedWebApp::InstallChecked(
    Profile* profile) {
  auto result = Install(profile);
  CHECK(result.has_value()) << result.error();
  return *result;
}

base::expected<IsolatedWebAppUrlInfo, std::string>
ScopedProxyIsolatedWebApp::Install(Profile* profile) {
  return Install(profile,
                 web_package::SignedWebBundleId::CreateRandomForDevelopment());
}

base::expected<IsolatedWebAppUrlInfo, std::string>
ScopedProxyIsolatedWebApp::Install(
    Profile* profile,
    const web_package::SignedWebBundleId& web_bundle_id) {
  return ::web_app::Install(
      profile, web_bundle_id,
      DevModeProxy{.proxy_url = proxy_server_->GetOrigin()});
}

ManifestBuilder::ManifestBuilder()
    : name_("Test App"),
      version_("0.0.1"),
      start_url_("/"),
      permissions_policy_{{"cross-origin-isolated", {"self"}}},
      icon_paths_{"/icon.png"} {}

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

ManifestBuilder& ManifestBuilder::AddPermissionsPolicy(
    std::string_view name,
    std::vector<std::string> value) {
  permissions_policy_[std::string(name)] = value;
  return *this;
}

ManifestBuilder& ManifestBuilder::AddIcon(std::string_view resource_path) {
  icon_paths_.push_back(std::string(resource_path));
  return *this;
}

const std::string& ManifestBuilder::start_url() const {
  return start_url_;
}

const std::vector<std::string>& ManifestBuilder::icon_paths() const {
  return icon_paths_;
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
    for (const auto& value : policy.second) {
      values.Append(value);
    }
    policies.Set(policy.first, std::move(values));
  }
  json.Set("permissions_policy", std::move(policies));

  base::Value::List icons;
  for (const auto& icon_path : icon_paths_) {
    // For now we just hardcode the icon size to 256x256.
    icons.Append(base::Value::Dict()
                     .Set("src", icon_path)
                     .Set("sizes", "256x256")
                     .Set("type", "image/png"));
  }
  json.Set("icons", std::move(icons));

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

  for (const auto& icon_path : icon_paths_) {
    blink::Manifest::ImageResource icon;
    icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
    icon.src = base_url.Resolve(icon_path);
    icon.type = u"image/png";
    icon.sizes.push_back(gfx::Size(256, 256));
    manifest->icons.push_back(icon);
  }

  // Permissions policy isn't included here as it's not needed by anything
  // yet and is tricky to parse.

  return manifest;
}

IsolatedWebAppBuilder::Resource::Resource(
    const IsolatedWebAppBuilder::Headers& headers,
    const IsolatedWebAppBuilder::ResourceBody& body)
    : headers_(headers), body_(body) {}

IsolatedWebAppBuilder::Resource::Resource(
    const IsolatedWebAppBuilder::Resource&) = default;
IsolatedWebAppBuilder::Resource::~Resource() = default;

scoped_refptr<net::HttpResponseHeaders>
IsolatedWebAppBuilder::Resource::headers() const {
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

  if (!has_content_type && absl::holds_alternative<base::FilePath>(body_)) {
    http_headers->AddHeader(
        net::HttpRequestHeaders::kContentType,
        net::test_server::GetContentType(absl::get<base::FilePath>(body_)));
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
  AddImageAsPng("/icon.png", CreateSquareIcon(256, SK_ColorBLUE));
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
    const Headers& headers) {
  CHECK(resource_path != kManifestPath)
      << "The manifest must be specified through the ManifestBuilder";
  resources_.insert_or_assign(std::string(resource_path),
                              Resource(headers, std::string(content)));
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
  CHECK(base::PathExists(file_path)) << file_path << " does not exist";
  resources_.insert_or_assign(std::string(resource_path),
                              Resource(headers, file_path));
  return *this;
}

IsolatedWebAppBuilder& IsolatedWebAppBuilder::AddFolderFromDisk(
    std::string_view resource_path,
    const base::FilePath& folder_path) {
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
    const std::string& chrome_test_data_relative_path) {
  base::FilePath absolute_path =
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data"))
          .Append(
              base::FilePath::FromUTF8Unsafe(chrome_test_data_relative_path));
  return AddFolderFromDisk(resource_path, absolute_path);
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
  return BuildBundle(web_package::WebBundleSigner::KeyPair::CreateRandom());
}

std::unique_ptr<ScopedBundledIsolatedWebApp> IsolatedWebAppBuilder::BuildBundle(
    const web_package::WebBundleSigner::KeyPair& key_pair) {
  return std::make_unique<ScopedBundledIsolatedWebApp>(
      web_package::SignedWebBundleId::CreateForEd25519PublicKey(
          key_pair.public_key),
      BuildInMemoryBundle(key_pair), manifest_builder_);
}

std::vector<uint8_t> IsolatedWebAppBuilder::BuildInMemoryBundle(
    const web_package::WebBundleSigner::KeyPair& key_pair) {
  Validate();
  web_package::WebBundleBuilder builder;
  for (const auto& resource : resources_) {
    scoped_refptr<net::HttpResponseHeaders> headers = resource.second.headers();

    web_package::WebBundleBuilder::Headers bundle_headers = {
        {":status", "200"}};
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

  return web_package::WebBundleSigner::SignBundle(builder.CreateBundle(),
                                                  {key_pair});
}

void IsolatedWebAppBuilder::Validate() {
  CHECK(resources_.find(manifest_builder_.start_url()) != resources_.end())
      << "Resource at 'start_url' (" << manifest_builder_.start_url()
      << ") does not exist";

  for (const auto& icon_path : manifest_builder_.icon_paths()) {
    CHECK(resources_.find(icon_path) != resources_.end())
        << "Icon at '" << icon_path << "' does not exist";
  }
}

// static
std::unique_ptr<net::test_server::HttpResponse>
IsolatedWebAppBuilder::HandleRequest(
    const ManifestBuilder& manifest_builder,
    const std::map<std::string, Resource>& resources,
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  std::string path = request.GetURL().path();
  if (path == kManifestPath) {
    response->set_code(net::HTTP_OK);
    response->set_content_type("application/manifest+json");
    response->set_content(manifest_builder.ToJson());
  } else if (const auto resource = resources.find(path);
             resource != resources.end()) {
    response->set_code(net::HTTP_OK);
    response->set_content(resource->second.body());

    scoped_refptr<net::HttpResponseHeaders> headers =
        resource->second.headers();

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
