// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/devtools_ui_data_source.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/url_data_source.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kDevToolsUITestFrontEndUrl[] = "/devtools_app.html";
constexpr char kDevToolsUITest404Response[] = "HTTP/1.1 404 Not Found";

GURL DevToolsUrl() {
  return GURL(base::StrCat({content::kChromeDevToolsScheme,
                            url::kStandardSchemeSeparator,
                            chrome::kChromeUIDevToolsHost}));
}

std::string DevToolsBundledPath(const std::string& path) {
  return base::StrCat({chrome::kChromeUIDevToolsBundledPath, path});
}

std::string DevToolsRemotePath(const std::string& path) {
  return base::StrCat({chrome::kChromeUIDevToolsRemotePath, path});
}

std::string DevToolsCustomPath(const std::string& path) {
  return base::StrCat({chrome::kChromeUIDevToolsCustomPath, path});
}

}  // namespace

class TestDevToolsDataSource : public DevToolsDataSource {
 public:
  TestDevToolsDataSource() : DevToolsDataSource(nullptr) {}
  ~TestDevToolsDataSource() override {}

  void StartNetworkRequest(
      const GURL& url,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      int load_flags,
      const GotDataCallback& callback) override {
    std::string result = "url: " + url.spec();
    callback.Run(base::RefCountedString::TakeString(&result));
  }

  void StartFileRequest(const std::string& path,
                        const GotDataCallback& callback) override {
    std::string result = "file: " + path;
    callback.Run(base::RefCountedString::TakeString(&result));
  }
};

class DevToolsUIDataSourceTest : public testing::Test {
 protected:
  DevToolsUIDataSourceTest() {}
  ~DevToolsUIDataSourceTest() override = default;

  void SetUp() override {
    devtools_data_source_ = std::make_unique<TestDevToolsDataSource>();
  }

  void TearDown() override { devtools_data_source_.reset(); }

  TestDevToolsDataSource* data_source() const {
    return devtools_data_source_.get();
  }

  bool data_received() const { return data_received_; }

  std::string data() const { return data_; }

  // TODO(crbug/1009127): pass in GURL instead.
  void StartRequest(const std::string& path) {
    data_received_ = false;
    data_.clear();
    std::string trimmed_path = path.substr(1);
    content::WebContents::Getter wc_getter;
    data_source()->StartDataRequest(
        GURL("chrome://any-host/" + trimmed_path), std::move(wc_getter),
        base::BindRepeating(&DevToolsUIDataSourceTest::OnDataReceived,
                            base::Unretained(this)));
  }

 private:
  void OnDataReceived(scoped_refptr<base::RefCountedMemory> bytes) {
    data_received_ = true;
    if (bytes.get() != nullptr) {
      data_ = base::StringPiece(reinterpret_cast<const char*>(bytes->front()),
                                bytes->size())
                  .as_string();
    }
  }

  std::unique_ptr<TestDevToolsDataSource> devtools_data_source_;
  bool data_received_ = false;
  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsUIDataSourceTest);
};

// devtools/bundled path.

TEST_F(DevToolsUIDataSourceTest, TestDevToolsBundledURL) {
  const GURL path =
      DevToolsUrl().Resolve(DevToolsBundledPath(kDevToolsUITestFrontEndUrl));
  StartRequest(path.path());
  EXPECT_TRUE(data_received());
  EXPECT_FALSE(data().empty());
}

TEST_F(DevToolsUIDataSourceTest, TestDevToolsBundledURLWithQueryParam) {
  const GURL path =
      DevToolsUrl().Resolve(DevToolsBundledPath(kDevToolsUITestFrontEndUrl));
  StartRequest(path.path() + "?foo");
  EXPECT_TRUE(data_received());
  EXPECT_FALSE(data().empty());
}

TEST_F(DevToolsUIDataSourceTest, TestDevToolsBundledURLWithSwitch) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kCustomDevtoolsFrontend, "file://tmp/");
  const GURL path =
      DevToolsUrl().Resolve(DevToolsBundledPath(kDevToolsUITestFrontEndUrl));
  StartRequest(path.path());
  EXPECT_TRUE(data_received());
  EXPECT_EQ(data(), "file: devtools_app.html");
}

TEST_F(DevToolsUIDataSourceTest, TestDevToolsInvalidBundledURL) {
  const GURL path =
      DevToolsUrl().Resolve(DevToolsBundledPath("invalid_devtools_app.html"));
  StartRequest(path.path());
  EXPECT_TRUE(data_received());
  ASSERT_TRUE(base::StartsWith(data(), kDevToolsUITest404Response,
                               base::CompareCase::SENSITIVE));
}

TEST_F(DevToolsUIDataSourceTest, TestDevToolsInvalidBundledURLWithQueryParam) {
  const GURL path =
      DevToolsUrl().Resolve(DevToolsBundledPath("invalid_devtools_app.html"));
  StartRequest(path.path() + "?foo");
  EXPECT_TRUE(data_received());
  ASSERT_TRUE(base::StartsWith(data(), kDevToolsUITest404Response,
                               base::CompareCase::SENSITIVE));
}

// devtools/blank path

TEST_F(DevToolsUIDataSourceTest, TestDevToolsBlankURL) {
  const GURL path = DevToolsUrl().Resolve(chrome::kChromeUIDevToolsBlankPath);
  StartRequest(path.path());
  EXPECT_TRUE(data_received());
  EXPECT_TRUE(data().empty());
}

TEST_F(DevToolsUIDataSourceTest, TestDevToolsBlankURLWithQueryParam) {
  const GURL path = DevToolsUrl().Resolve(chrome::kChromeUIDevToolsBlankPath);
  StartRequest(path.path() + "?foo");
  EXPECT_TRUE(data_received());
  EXPECT_TRUE(data().empty());
}

// devtools/remote path

TEST_F(DevToolsUIDataSourceTest, TestDevToolsRemoteURL) {
  const GURL path =
      DevToolsUrl().Resolve(DevToolsRemotePath(kDevToolsUITestFrontEndUrl));
  StartRequest(path.path());
  EXPECT_TRUE(data_received());
  EXPECT_EQ(
      data(),
      "url: https://chrome-devtools-frontend.appspot.com/devtools_app.html");
}

TEST_F(DevToolsUIDataSourceTest, TestDevToolsRemoteURLWithQueryParam) {
  const GURL path =
      DevToolsUrl().Resolve(DevToolsRemotePath(kDevToolsUITestFrontEndUrl));
  StartRequest(path.path() + "?foo");
  EXPECT_TRUE(data_received());
  ASSERT_TRUE(base::StartsWith(data(), kDevToolsUITest404Response,
                               base::CompareCase::SENSITIVE));
}

// devtools/custom path.

TEST_F(DevToolsUIDataSourceTest, TestDevToolsCustomURLWithNoSwitch) {
  const GURL path =
      DevToolsUrl().Resolve(DevToolsCustomPath(kDevToolsUITestFrontEndUrl));
  StartRequest(path.path());
  EXPECT_TRUE(data_received());
  ASSERT_TRUE(base::StartsWith(data(), kDevToolsUITest404Response,
                               base::CompareCase::SENSITIVE));
}

TEST_F(DevToolsUIDataSourceTest, TestDevToolsCustomURLWithSwitch) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kCustomDevtoolsFrontend, "http://localhost:8090/front_end/");
  const GURL path =
      DevToolsUrl().Resolve(DevToolsCustomPath(kDevToolsUITestFrontEndUrl));
  StartRequest(path.path());
  EXPECT_TRUE(data_received());
  EXPECT_EQ(data(), "url: http://localhost:8090/front_end/devtools_app.html");
}

TEST_F(DevToolsUIDataSourceTest, TestDevToolsCustomURLWithSwitchAndQueryParam) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kCustomDevtoolsFrontend, "http://localhost:8090/front_end/");
  const GURL path =
      DevToolsUrl().Resolve(DevToolsCustomPath(kDevToolsUITestFrontEndUrl));
  StartRequest(path.path() + "?foo");
  EXPECT_TRUE(data_received());
  EXPECT_EQ(data(),
            "url: http://localhost:8090/front_end/devtools_app.html?foo");
}

#if !DCHECK_IS_ON()
TEST_F(DevToolsUIDataSourceTest,
       TestDevToolsCustomURLWithSwitchAndInvalidServerURL) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kCustomDevtoolsFrontend, "invalid-server-url");
  const GURL path =
      DevToolsUrl().Resolve(DevToolsCustomPath(kDevToolsUITestFrontEndUrl));
  StartRequest(path.path());
  EXPECT_TRUE(data_received());
  ASSERT_TRUE(base::StartsWith(data(), kDevToolsUITest404Response,
                               base::CompareCase::SENSITIVE));
}
#endif

// devtools path (i.e. no route specified).

TEST_F(DevToolsUIDataSourceTest, TestDevToolsNoRoute) {
  const GURL path = DevToolsUrl().Resolve(kDevToolsUITestFrontEndUrl);
  StartRequest(path.path());
  EXPECT_TRUE(data_received());
  ASSERT_TRUE(base::StartsWith(data(), kDevToolsUITest404Response,
                               base::CompareCase::SENSITIVE));
}

TEST_F(DevToolsUIDataSourceTest, TestDevToolsNoRouteWithSwitch) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kCustomDevtoolsFrontend, "invalid-server-url");
  const GURL path = DevToolsUrl().Resolve(kDevToolsUITestFrontEndUrl);
  StartRequest(path.path());
  EXPECT_TRUE(data_received());
  ASSERT_TRUE(base::StartsWith(data(), kDevToolsUITest404Response,
                               base::CompareCase::SENSITIVE));
}
