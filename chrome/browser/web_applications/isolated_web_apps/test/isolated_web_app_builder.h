// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_ISOLATED_WEB_APP_BUILDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_ISOLATED_WEB_APP_BUILDER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "net/http/http_status_code.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-forward.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"
#include "ui/gfx/geometry/size.h"

class Profile;
class SkBitmap;

namespace net::test_server {
class EmbeddedTestServer;
class HttpResponse;
struct HttpRequest;
}  // namespace net::test_server

namespace net {
using EmbeddedTestServer = test_server::EmbeddedTestServer;
class HttpResponseHeaders;
}  // namespace net

namespace url {
class Origin;
}  // namespace url

namespace web_app {

// A builder for a subset of the Web Manifest spec.
class ManifestBuilder {
 public:
  struct IconMetadata {
    std::string resource_path;
    gfx::Size size;
    std::string content_type;
  };

  struct PermissionsPolicy {
    PermissionsPolicy(bool wildcard,
                      bool self,
                      std::vector<url::Origin> origins);
    PermissionsPolicy(const PermissionsPolicy&);
    ~PermissionsPolicy();

    bool wildcard;
    bool self;
    std::vector<url::Origin> origins;
  };

  // Mime type to vector of file extensions.
  using FileHandlerAccept = std::map<std::string, std::vector<std::string>>;

  // Creates the following default manifest:
  // {
  //   name: "Test App",
  //   version: "0.0.1",
  //   id: "/",
  //   scope: "/",
  //   start_url: "/",
  //   permissions_policy: {
  //     cross-origin-isolated: ["self"]
  //   },
  // }
  ManifestBuilder();
  ManifestBuilder(const ManifestBuilder&);

  ~ManifestBuilder();

  ManifestBuilder& SetName(std::string_view name);
  ManifestBuilder& SetVersion(std::string_view version);
  ManifestBuilder& SetStartUrl(std::string_view start_url);
  ManifestBuilder& AddIcon(std::string_view resource_path,
                           gfx::Size size,
                           std::string_view content_type);

  ManifestBuilder& AddPermissionsPolicyWildcard(
      blink::mojom::PermissionsPolicyFeature feature);
  ManifestBuilder& AddPermissionsPolicy(
      blink::mojom::PermissionsPolicyFeature feature,
      bool self,
      std::vector<url::Origin> origins);

  ManifestBuilder& AddProtocolHandler(std::string_view protocol,
                                      std::string_view url);
  ManifestBuilder& AddFileHandler(std::string_view action,
                                  const FileHandlerAccept& accept);

  // TODO: Other manifest fields like share_target as needed by tests.
  const std::string& start_url() const;
  const std::vector<IconMetadata>& icons() const;
  base::Version version() const;

  std::string ToJson() const;
  blink::mojom::ManifestPtr ToBlinkManifest(
      const url::Origin& app_origin) const;

 private:
  std::string name_;
  std::string version_;
  std::string start_url_;
  std::vector<IconMetadata> icons_;
  std::map<blink::mojom::PermissionsPolicyFeature, PermissionsPolicy>
      permissions_policy_;
  std::vector<std::pair<std::string, std::string>> protocol_handlers_;
  std::map<std::string, FileHandlerAccept> file_handlers_;
};

class BundledIsolatedWebApp {
 public:
  BundledIsolatedWebApp(const web_package::SignedWebBundleId& web_bundle_id,
                        const std::vector<uint8_t> serialized_bundle,
                        const base::FilePath path,
                        ManifestBuilder manifest_builder);

  virtual ~BundledIsolatedWebApp();

  const base::FilePath& path() const { return path_; }

  const web_package::SignedWebBundleId& web_bundle_id() const {
    return web_bundle_id_;
  }

  base::Version version() const { return manifest_builder_.version(); }

  std::string GetBundleData() const;

  // Saves this app's signing key in Chrome's list of trusted keys, which will
  // allow the app to be installed with dev mode disabled.
  void TrustSigningKey();

  void FakeInstallPageState(Profile* profile);

  IsolatedWebAppUrlInfo InstallChecked(Profile* profile);

  base::expected<IsolatedWebAppUrlInfo, std::string> Install(Profile* profile);
  base::expected<IsolatedWebAppUrlInfo, std::string> TrustBundleAndInstall(
      Profile* profile);

 private:
  web_package::SignedWebBundleId web_bundle_id_;
  base::FilePath path_;
  ManifestBuilder manifest_builder_;
};

class ScopedBundledIsolatedWebApp : public BundledIsolatedWebApp {
 public:
  static std::unique_ptr<ScopedBundledIsolatedWebApp> Create(
      const web_package::SignedWebBundleId& web_bundle_id,
      const std::vector<uint8_t> serialized_bundle,
      ManifestBuilder manifest_builder);

  ~ScopedBundledIsolatedWebApp() override;

 private:
  ScopedBundledIsolatedWebApp(
      const web_package::SignedWebBundleId& web_bundle_id,
      const std::vector<uint8_t> serialized_bundle,
      base::ScopedTempFile bundle_file,
      ManifestBuilder manifest_builder);

  base::ScopedTempFile bundle_file_;
};

class ScopedProxyIsolatedWebApp {
 public:
  explicit ScopedProxyIsolatedWebApp(
      std::unique_ptr<net::EmbeddedTestServer> proxy_server,
      std::optional<ManifestBuilder> manifest_builder = std::nullopt);

  ~ScopedProxyIsolatedWebApp();

  net::EmbeddedTestServer& proxy_server() { return *proxy_server_; }

  IsolatedWebAppUrlInfo InstallChecked(Profile* profile);

  base::expected<IsolatedWebAppUrlInfo, std::string> Install(Profile* profile);

  base::expected<IsolatedWebAppUrlInfo, std::string> Install(
      Profile* profile,
      const web_package::SignedWebBundleId& web_bundle_id);

 private:
  std::unique_ptr<net::EmbeddedTestServer> proxy_server_;
  std::optional<ManifestBuilder> manifest_builder_;
};

// A builder for Isolated Web Apps that supports adding resources from disk
// (typically from //chrome/test/data), or from strings provided by the test,
// and allows callers to create a dev proxy server or signed web bundle file
// containing the configured resources.
//
// If multiple resources are registered for the same path, the latest
// registration will "win". This allows callers to for example add a test app
// from a directory with `AddFolderFromDisk`, and then override a specific
// file with a subsequent call to `AddResource`.
class IsolatedWebAppBuilder {
 public:
  struct Header {
    std::string name;
    std::string value;
  };
  using Headers = std::vector<Header>;

  // Initializes the builder with the specified manifest and some common
  // resources such as a default text/html file at '/'.
  //
  // The following resources will be present in the app:
  //   * /
  //   * /manifest.webmanifest
  //   * /icon.png
  explicit IsolatedWebAppBuilder(const ManifestBuilder& manifest_builder);
  IsolatedWebAppBuilder(const IsolatedWebAppBuilder&);

  ~IsolatedWebAppBuilder();

  IsolatedWebAppBuilder& AddResource(std::string_view resource_path,
                                     std::string_view content,
                                     std::string_view content_type);

  // Adds a resource to the app with the given content and headers. The mime
  // type will NOT be inferred from the path or content, so Content-Type should
  // be included in the list of headers.
  IsolatedWebAppBuilder& AddResource(std::string_view resource_path,
                                     std::string_view content,
                                     const Headers& headers,
                                     net::HttpStatusCode status = net::HTTP_OK);

  // Adds a text/html type resource to the app.
  IsolatedWebAppBuilder& AddHtml(std::string_view resource_path,
                                 std::string_view content);

  // Adds a text/javascript type resource to the app.
  IsolatedWebAppBuilder& AddJs(std::string_view resource_path,
                               std::string_view content);

  // Adds a image/png type resource to the app, and adds it as an icon in the
  // manifest.
  IsolatedWebAppBuilder& AddIconAsPng(std::string_view resource_path,
                                      const SkBitmap& image);

  // Adds a image/png type resource to the app.
  IsolatedWebAppBuilder& AddImageAsPng(std::string_view resource_path,
                                       const SkBitmap& image);

  // Adds the contents of a file from disk to the app. If a
  // `file_path`.mock-http-headers sibling file exists, the headers within
  // that file will be used for this resource. If no .mock-http-headers file is
  // present, the mime type will be inferred from `file_path`'s extension.
  // Headers specified in `headers` will be appended to those from a
  // .mock-http-headers file.
  IsolatedWebAppBuilder& AddFileFromDisk(std::string_view resource_path,
                                         const base::FilePath& file_path,
                                         const Headers& headers = {});

  // Adds file specified by a path relative to //chrome/test/data to the app.
  IsolatedWebAppBuilder& AddFileFromDisk(
      std::string_view resource_path,
      std::string_view chrome_test_data_relative_path,
      const Headers& headers = {});

  // Recursively adds the contents of the `folder_path` directory on disk to the
  // app, using `resource_path` as the base path within the app.
  //
  // .mock-http-headers sibling files are supported as described in
  // `AddFileFromDisk`.
  IsolatedWebAppBuilder& AddFolderFromDisk(std::string_view resource_path,
                                           const base::FilePath& folder_path);

  // Recursively adds the contents of a folder specified by a path relative to
  // //chrome/test/data to the app.
  IsolatedWebAppBuilder& AddFolderFromDisk(
      std::string_view resource_path,
      std::string_view chrome_test_data_relative_path);

  IsolatedWebAppBuilder& RemoveResource(std::string_view resource_path);

  // Creates and starts a new server that will serve a snapshot of the app's
  // contents as they were when this function was called.
  [[nodiscard]] std::unique_ptr<ScopedProxyIsolatedWebApp>
  BuildAndStartProxyServer();

  // Creates and signs a .swbn file on disk containing the app's contents. A
  // random signing key will be created and used to sign the bundle.
  [[nodiscard]] std::unique_ptr<ScopedBundledIsolatedWebApp> BuildBundle();

  // Creates and signs a .swbn file on disk containing the app's contents.
  [[nodiscard]] std::unique_ptr<ScopedBundledIsolatedWebApp> BuildBundle(
      const web_package::test::KeyPair& key_pair);

  // Creates and signs a .swbn file on disk containing the app's contents.
  [[nodiscard]] std::unique_ptr<ScopedBundledIsolatedWebApp> BuildBundle(
      const web_package::SignedWebBundleId& web_bundle_id,
      const web_package::test::KeyPairs& key_pairs);

  // Creates and signs a .swbn file on disk containing the app's contents. The
  // location of the bundle must be provided in `bundle_path`. A random signing
  // key will be created and used to sign the bundle.
  std::unique_ptr<BundledIsolatedWebApp> BuildBundle(
      const base::FilePath& bundle_path);

  // Creates and signs a .swbn file on disk containing the app's contents. The
  // location of the bundle must be provided in `bundle_path`.
  std::unique_ptr<BundledIsolatedWebApp> BuildBundle(
      const base::FilePath& bundle_path,
      const web_package::test::KeyPair& key_pair);

  // Creates and signs a .swbn file on disk containing the app's contents. The
  // location of the bundle must be provided in `bundle_path`.
  std::unique_ptr<BundledIsolatedWebApp> BuildBundle(
      const base::FilePath& bundle_path,
      const web_package::SignedWebBundleId& web_bundle_id,
      const web_package::test::KeyPairs& key_pairs);

 private:
  using ResourceBody = absl::variant<base::FilePath, std::string>;

  class Resource {
   public:
    Resource(net::HttpStatusCode status,
             const Headers& headers,
             const ResourceBody& body);
    Resource(const Resource&);
    ~Resource();

    net::HttpStatusCode status() const { return status_; }
    scoped_refptr<net::HttpResponseHeaders> headers(
        std::string_view resource_path) const;
    std::string body() const;

   private:
    net::HttpStatusCode status_;
    Headers headers_;
    ResourceBody body_;
  };

  static std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const ManifestBuilder& manifest_builder,
      const std::map<std::string, Resource>& resources,
      const net::test_server::HttpRequest& request);

  std::vector<uint8_t> BuildInMemoryBundle(
      const web_package::SignedWebBundleId& web_bundle_id,
      const web_package::test::KeyPairs& key_pairs);

  void Validate();

  ManifestBuilder manifest_builder_;
  // Maps relative path to resource body.
  std::map<std::string, Resource> resources_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_ISOLATED_WEB_APP_BUILDER_H_
