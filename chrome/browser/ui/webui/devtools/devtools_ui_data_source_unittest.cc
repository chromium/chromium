// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/devtools/devtools_ui_data_source.h"

#include <memory>
#include <string_view>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
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
      GotDataCallback callback) override {
    std::string result = "url: " + url.spec();
    std::move(callback).Run(
        base::MakeRefCounted<base::RefCountedString>(std::move(result)));
  }

  void StartFileRequest(const std::string& path,
                        GotDataCallback callback) override {
    std::string result = "file: " + path;
    std::move(callback).Run(
        base::MakeRefCounted<base::RefCountedString>(std::move(result)));
  }
};

class DevToolsUIDataSourceTest : public testing::Test {
 public:
  DevToolsUIDataSourceTest(const DevToolsUIDataSourceTest&) = delete;
  DevToolsUIDataSourceTest& operator=(const DevToolsUIDataSourceTest&) = delete;

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

  // TODO(crbug.com/40050262): pass in GURL instead.
  void StartRequest(const std::string& path) {
    data_received_ = false;
    data_.clear();
    std::string trimmed_path = path.substr(1);
    content::WebContents::Getter wc_getter;
    data_source()->StartDataRequest(
        GURL("chrome://any-host/" + trimmed_path), std::move(wc_getter),
        base::BindOnce(&DevToolsUIDataSourceTest::OnDataReceived,
                       base::Unretained(this)));
  }

 private:
  void OnDataReceived(scoped_refptr<base::RefCountedMemory> bytes) {
    data_received_ = true;
    if (bytes.get()) {
      data_ = std::string(std::string_view(
          reinterpret_cast<const char*>(bytes->front()), bytes->size()));
    }
  }

  std::unique_ptr<TestDevToolsDataSource> devtools_data_source_;
  bool data_received_ = false;
  std::string data_;
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

TEST_F(DevToolsUIDataSourceTest, TestDevToolsBundledFileURLWithSwitch) {
#if BUILDFLAG(IS_WIN)
  const char* flag_value = "file://C:/tmp/";
#else
  const char* flag_value = "file://tmp/";
#endif
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kCustomDevtoolsFrontend, flag_value);
  const GURL path =
      DevToolsUrl().Resolve(DevToolsBundledPath(kDevToolsUITestFrontEndUrl));
  StartRequest(path.path());
  EXPECT_TRUE(data_received());
  EXPECT_EQ(data(), "file: devtools_app.html");
}

TEST_F(DevToolsUIDataSourceTest, TestDevToolsBundledRemoteURLWithSwitch) {
  const char* flag_value = "http://example.com/example/path/";
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kCustomDevtoolsFrontend, flag_value);
  const GURL path =
      DevToolsUrl().Resolve(DevToolsBundledPath(kDevToolsUITestFrontEndUrl));
  StartRequest(path.path());
  EXPECT_TRUE(data_received());
  EXPECT_EQ(data(), "url: http://example.com/example/path/devtools_app.html");
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

TEST_F(DevToolsUIDataSourceTest, TestDevToolsRemoteURLWithSwitch) {
  const char* flag_value = "http://example.com/example/path/";
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kCustomDevtoolsFrontend, flag_value);
  const GURL path =
      DevToolsUrl().Resolve(DevToolsRemotePath(kDevToolsUITestFrontEndUrl));
  StartRequest(path.path());
  EXPECT_TRUE(data_received());
  EXPECT_EQ(data(), "url: http://example.com/example/path/devtools_app.html");
}

TEST_F(DevToolsUIDataSourceTest, TestDevToolsRemoteFileURLWithSwitch) {
#if BUILDFLAG(IS_WIN)
  const char* flag_value = "file://C:/tmp/";
#else
  const char* flag_value = "file://tmp/";
#endif
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kCustomDevtoolsFrontend, flag_value);
  const GURL path =
      DevToolsUrl().Resolve(DevToolsRemotePath(kDevToolsUITestFrontEndUrl));
  StartRequest(path.path());
  EXPECT_TRUE(data_received());
  EXPECT_EQ(data(), "file: devtools_app.html");
}

TEST_F(DevToolsUIDataSourceTest,
       TestDevToolsRemoteFileURLWithSwitchAndServeRevParameters) {
#if BUILDFLAG(IS_WIN)
  const char* flag_value = "file://C:/tmp/";
#else
  const char* flag_value = "file://tmp/";
#endif
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kCustomDevtoolsFrontend, flag_value);
  const GURL path = DevToolsUrl().Resolve(
      DevToolsRemotePath("/serve_rev/@76e4c1bb2ab4671b8beba3444e61c0f17584b2fc/"
                         "devtools_app.html"));
  StartRequest(path.path());
  EXPECT_TRUE(data_received());
  EXPECT_EQ(data(), "file: devtools_app.html");
}

TEST_F(DevToolsUIDataSourceTest,
       TestDevToolsRemoteFileURLWithSwitchAndServeFileParameters) {
#if BUILDFLAG(IS_WIN)
  const char* flag_value = "file://C:/tmp/";
#else
  const char* flag_value = "file://tmp/";
#endif
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kCustomDevtoolsFrontend, flag_value);
  const GURL path = DevToolsUrl().Resolve(DevToolsRemotePath(
      "/serve_file/@76e4c1bb2ab4671b8beba3444e61c0f17584b2fc/"
      "devtools_app.html"));
  StartRequest(path.path());
  EXPECT_TRUE(data_received());
  EXPECT_EQ(data(), "file: devtools_app.html");
}

TEST_F(DevToolsUIDataSourceTest,
       TestDevToolsRemoteFileURLWithSwitchAndServeInternalFileParameters) {
#if BUILDFLAG(IS_WIN)
  const char* flag_value = "file://C:/tmp/";
#else
  const char* flag_value = "file://tmp/";
#endif
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kCustomDevtoolsFrontend, flag_value);
  const GURL path = DevToolsUrl().Resolve(DevToolsRemotePath(
      "/serve_internal_file/@76e4c1bb2ab4671b8beba3444e61c0f17584b2fc/"
      "devtools_app.html"));
  StartRequest(path.path());
  EXPECT_TRUE(data_received());
  EXPECT_EQ(data(), "file: devtools_app.html");
}

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

class DevToolsUIDataSourceWithTaskEnvTest : public testing::Test {
 public:
  DevToolsUIDataSourceWithTaskEnvTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(DevToolsUIDataSourceWithTaskEnvTest,
       GotDataCallbackOwnsDevToolsDataSource) {
  scoped_refptr<network::SharedURLLoaderFactory> factory =
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>();
  DevToolsDataSource* data_source = new DevToolsDataSource(factory);

  DevToolsDataSource::GotDataCallback callback = base::BindOnce(
      [](DevToolsDataSource* data_source,
         scoped_refptr<base::RefCountedMemory> payload) {
        // Do nothing in the callback.
      },
      base::Owned(data_source));

  // `callback` controls the life-time of the data_source now, so data_source is
  // deleted after the callback is done running. This is similar to what
  // WebUIURLLoaderFactory is doing.

  const GURL path =
      DevToolsUrl().Resolve(DevToolsRemotePath(kDevToolsUITestFrontEndUrl));
  content::WebContents::Getter wc_getter;
  data_source->StartDataRequest(path, std::move(wc_getter),
                                std::move(callback));
}
