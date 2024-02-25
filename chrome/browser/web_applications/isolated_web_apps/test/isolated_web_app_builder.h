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
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

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
  //   icons: [
  //     {
  //       src: "/icon.png",
  //       sizes: "256x256",
  //       type: "image/png"
  //     }
  //   ]
  // }
  ManifestBuilder();
  ManifestBuilder(const ManifestBuilder&);

  ~ManifestBuilder();

  ManifestBuilder& SetName(std::string_view name);
  ManifestBuilder& SetVersion(std::string_view version);
  ManifestBuilder& SetStartUrl(std::string_view start_url);
  ManifestBuilder& AddPermissionsPolicy(std::string_view name,
                                        std::vector<std::string> value);
  ManifestBuilder& AddIcon(std::string_view resource_path);

  // TODO: Other manifest fields like file_handlers, protocol_handlers,
  // share_target as needed by tests.

  const std::string& start_url() const;
  const std::vector<std::string>& icon_paths() const;

  std::string ToJson() const;
  blink::mojom::ManifestPtr ToBlinkManifest(
      const url::Origin& app_origin) const;

 private:
  std::string name_;
  std::string version_;
  std::string start_url_;
  std::map<std::string, std::vector<std::string>> permissions_policy_;
  std::vector<std::string> icon_paths_;
};

class ScopedBundledIsolatedWebApp {
 public:
  ScopedBundledIsolatedWebApp(
      const web_package::SignedWebBundleId& web_bundle_id,
      const std::vector<uint8_t> serialized_bundle,
      std::optional<ManifestBuilder> manifest_builder = std::nullopt);

  ~ScopedBundledIsolatedWebApp();

  const base::FilePath& path() const { return bundle_file_.path(); }

  const web_package::SignedWebBundleId& web_bundle_id() const {
    return web_bundle_id_;
  }

  // Saves this app's signing key in Chrome's list of trusted keys, which will
  // allow the app to be installed with dev mode disabled.
  void TrustSigningKey();

  void FakeInstallPageState(Profile* profile);

  IsolatedWebAppUrlInfo InstallChecked(Profile* profile);

  base::expected<IsolatedWebAppUrlInfo, std::string> Install(Profile* profile);

 private:
  web_package::SignedWebBundleId web_bundle_id_;
  base::ScopedTempFile bundle_file_;
  std::optional<ManifestBuilder> manifest_builder_;
};

class ScopedProxyIsolatedWebApp {
 public:
  ScopedProxyIsolatedWebApp(
      std::unique_ptr<net::EmbeddedTestServer> proxy_server,
      std::optional<ManifestBuilder> manifest_builder = std::nullopt);

  ~ScopedProxyIsolatedWebApp();

  net::EmbeddedTestServer& proxy_server() { return *proxy_server_; }

  void FakeInstallPageState(
      Profile* profile,
      const web_package::SignedWebBundleId& web_bundle_id);

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
                                     const Headers& headers);

  // Adds a text/html type resource to the app.
  IsolatedWebAppBuilder& AddHtml(std::string_view resource_path,
                                 std::string_view content);

  // Adds a text/javascript type resource to the app.
  IsolatedWebAppBuilder& AddJs(std::string_view resource_path,
                               std::string_view content);

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
      const std::string& chrome_test_data_relative_path);

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
      const web_package::WebBundleSigner::KeyPair& key_pair);

  // Creates and signs a .swbn file and returns its serialized contents.
  //
  // Prefer the BuildBundle overloads that return a ScopedBundledIsolatedWebApp.
  std::vector<uint8_t> BuildInMemoryBundle(
      const web_package::WebBundleSigner::KeyPair& key_pair);

 private:
  using ResourceBody = absl::variant<base::FilePath, std::string>;

  class Resource {
   public:
    Resource(const Headers& headers, const ResourceBody& body);
    Resource(const Resource&);
    ~Resource();

    scoped_refptr<net::HttpResponseHeaders> headers() const;
    std::string body() const;

   private:
    Headers headers_;
    ResourceBody body_;
  };

  static std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const ManifestBuilder& manifest_builder,
      const std::map<std::string, Resource>& resources,
      const net::test_server::HttpRequest& request);

  void Validate();

  ManifestBuilder manifest_builder_;
  // Maps relative path to resource body.
  std::map<std::string, Resource> resources_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_ISOLATED_WEB_APP_BUILDER_H_
