// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/console_message_logger/headless_console_message_logger.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/test/scoped_logging_settings.h"
#include "build/build_config.h"
#include "content/public/browser/console_message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {
namespace {

class HeadlessConsoleMessageLoggerTest : public testing::Test {
 public:
  void SetUp() override {
    logging::SetLogMessageHandler([](int severity, const char* file, int line,
                                     size_t start,
                                     const std::string& str) -> bool {
      *log_string_ = str;
      return true;
    });

    logging::SetLogItems(/*enable_process_id=*/false,
                         /*enable_thread_id=*/false,
                         /*enable_timestamp=*/false,
                         /*enable_tickcount=*/false);
  }

  void TearDown() override {
    logging::SetLogMessageHandler(nullptr);
    log_string_->clear();
  }

 protected:
  const std::string& log_string() const { return *log_string_; }

 private:
  logging::ScopedLoggingSettings scoped_logging_settings_;
  static base::NoDestructor<std::string> log_string_;
};

base::NoDestructor<std::string> HeadlessConsoleMessageLoggerTest::log_string_;

TEST_F(HeadlessConsoleMessageLoggerTest, Basics) {
  headless::LogConsoleMessage(blink::mojom::ConsoleMessageLevel::kError,
                              u"foobar", 42, /*is_builtin_component=*/false,
                              u"test.js");

  const char kExpected[] =
#if BUILDFLAG(IS_CHROMEOS)
      R"(INFO components_unittests: [CONSOLE:42] "foobar", source: test.js (42)
)";
#else
      R"([INFO:CONSOLE:42] "foobar", source: test.js (42)
)";
#endif
  EXPECT_THAT(log_string(), testing::StrEq(kExpected));
}

TEST_F(HeadlessConsoleMessageLoggerTest, BuiltInComponentLogLevel) {
  // Log level for built-in components is respected.
  headless::LogConsoleMessage(blink::mojom::ConsoleMessageLevel::kError,
                              u"foobar", 42, /*is_builtin_component=*/true,
                              u"test.js");
  EXPECT_THAT(log_string(), testing::HasSubstr("ERROR"));
}

TEST_F(HeadlessConsoleMessageLoggerTest, DataUrl) {
  // Data URL is logged with data portion truncated.
  headless::LogConsoleMessage(blink::mojom::ConsoleMessageLevel::kError,
                              u"foobar", 42, /*is_builtin_component=*/false,
                              u"data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///"
                              u"yH5BAEAAAAALAAAAAABAAEAAAIBRAA7");
  EXPECT_THAT(log_string(),
              testing::HasSubstr(
                  R"(] "foobar", source: data:image/gif;base64... (42))"));
}

TEST_F(HeadlessConsoleMessageLoggerTest, MalformedDataUrl) {
  // Incomplete data URL is shortened to a minimum.
  headless::LogConsoleMessage(blink::mojom::ConsoleMessageLevel::kError,
                              u"foobar", 42, /*is_builtin_component=*/false,
                              u"data:image/gif;base64");
  EXPECT_THAT(log_string(),
              testing::HasSubstr(R"(] "foobar", source: data:... (42))"));
}

}  // namespace
}  // namespace headless
