// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/open_from_clipboard/clipboard_recent_content_ios.h"

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/task_environment.h"
#import "components/open_from_clipboard/clipboard_recent_content_impl_ios.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForCookiesTimeout;
using base::test::ios::kWaitForActionTimeout;

namespace {

UIImage* TestUIImage(UIColor* color = [UIColor redColor]) {
  CGRect frame = CGRectMake(0, 0, 1.0, 1.0);
  UIGraphicsBeginImageContext(frame.size);

  CGContextRef context = UIGraphicsGetCurrentContext();
  CGContextSetFillColorWithColor(context, color.CGColor);
  CGContextFillRect(context, frame);

  UIImage* image = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();

  return image;
}

void SetPasteboardImage(UIImage* image) {
  [[UIPasteboard generalPasteboard] setImage:image];
}

void SetPasteboardContent(const char* data) {
  [[UIPasteboard generalPasteboard]
               setValue:[NSString stringWithUTF8String:data]
      forPasteboardType:@"public.plain-text"];
}
const char kUnrecognizedURL[] = "bad://foo/";
const char kRecognizedURL[] = "good://bar/";
const char kRecognizedURL2[] = "good://bar/2";
const char kAppSpecificURL[] = "test://qux/";
const char kAppSpecificScheme[] = "test";
const char kRecognizedScheme[] = "good";
NSTimeInterval kLongerThanMaxAge = 60 * 60 * 7;
NSTimeInterval kMaxAge = 60 * 60 * 1;
}  // namespace

@interface ClipboardRecentContentImplIOSWithFakeUptime
    : ClipboardRecentContentImplIOS
@property(nonatomic) NSTimeInterval fakeUptime;

- (instancetype)initWithMaxAge:(NSTimeInterval)maxAge
             authorizedSchemes:(NSArray*)authorizedSchemes
                  userDefaults:(NSUserDefaults*)groupUserDefaults
         onlyUseClipboardAsync:(BOOL)onlyUseClipboardAsync
                        uptime:(NSTimeInterval)uptime;

@end

@implementation ClipboardRecentContentImplIOSWithFakeUptime

@synthesize fakeUptime = _fakeUptime;

- (instancetype)initWithMaxAge:(NSTimeInterval)maxAge
             authorizedSchemes:(NSSet*)authorizedSchemes
                  userDefaults:(NSUserDefaults*)groupUserDefaults
         onlyUseClipboardAsync:(BOOL)onlyUseClipboardAsync
                        uptime:(NSTimeInterval)uptime {
  self = [super initWithMaxAge:maxAge
             authorizedSchemes:authorizedSchemes
                  userDefaults:groupUserDefaults
         onlyUseClipboardAsync:onlyUseClipboardAsync
                      delegate:nil];
  if (self) {
    _fakeUptime = uptime;
  }
  return self;
}

- (NSTimeInterval)uptime {
  return self.fakeUptime;
}

@end

class ClipboardRecentContentIOSWithFakeUptime
    : public ClipboardRecentContentIOS {
 public:
  ClipboardRecentContentIOSWithFakeUptime(
      ClipboardRecentContentImplIOS* implementation)
      : ClipboardRecentContentIOS(implementation) {}
};

class ClipboardRecentContentIOSTest : public ::testing::Test {
 protected:
  ClipboardRecentContentIOSTest() {
    // By default, set that the device booted 10 days ago.
    ResetClipboardRecentContent(kAppSpecificScheme, base::Days(10));
  }

  void SimulateDeviceRestart() {
    ResetClipboardRecentContent(kAppSpecificScheme, base::Seconds(0));
  }

  void ResetClipboardRecentContent(const std::string& application_scheme,
                                   base::TimeDelta time_delta) {
    ClipboardRecentContentImplIOSWithFakeUptime*
        clipboard_content_implementation =
            [[ClipboardRecentContentImplIOSWithFakeUptime alloc]
                       initWithMaxAge:kMaxAge
                    authorizedSchemes:@[
                      base::SysUTF8ToNSString(kRecognizedScheme),
                      base::SysUTF8ToNSString(application_scheme)
                    ]
                         userDefaults:[NSUserDefaults standardUserDefaults]
                onlyUseClipboardAsync:NO
                               uptime:time_delta.InSecondsF()];

    clipboard_content_ =
        std::make_unique<ClipboardRecentContentIOSWithFakeUptime>(
            clipboard_content_implementation);

    // Keep a weak pointer to the ClipboardRecentContentImplIOS to allow
    // updating the fake pasteboard change date.
    clipboard_content_implementation_ = clipboard_content_implementation;
  }

  void SetStoredPasteboardChangeDate(NSDate* change_date) {
    clipboard_content_implementation_.lastPasteboardChangeDate = change_date;
    [clipboard_content_implementation_ saveToUserDefaults];
  }

 protected:
  std::unique_ptr<ClipboardRecentContentIOSWithFakeUptime> clipboard_content_;
  __weak ClipboardRecentContentImplIOSWithFakeUptime*
      clipboard_content_implementation_;

  base::test::TaskEnvironment task_environment_;

  void VerifyClipboardTypeExists(ClipboardContentType type, bool exists) {
    __block BOOL callback_called = NO;
    __block BOOL type_exists = NO;
    std::set<ClipboardContentType> types;
    types.insert(type);
    clipboard_content_->HasRecentContentFromClipboard(
        types, base::BindOnce(^(std::set<ClipboardContentType> found_types) {
          callback_called = YES;
          type_exists = found_types.find(type) != found_types.end();
        }));

    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForCookiesTimeout, ^bool {
      base::RunLoop().RunUntilIdle();
      return callback_called;
    }));
    EXPECT_EQ(exists, type_exists);
  }

  void VerifyClipboardURLExists(const char* expected_url) {
    VerifyClipboardTypeExists(ClipboardContentType::URL, true);

    __block BOOL callback_called = NO;
    __block std::optional<GURL> optional_gurl;
    clipboard_content_->GetRecentURLFromClipboard(
        base::BindOnce(^(std::optional<GURL> copied_url) {
          optional_gurl = copied_url;
          callback_called = YES;
        }));
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForCookiesTimeout, ^bool {
      base::RunLoop().RunUntilIdle();
      return callback_called;
    }));
    ASSERT_TRUE(optional_gurl.has_value());
    EXPECT_STREQ(expected_url, optional_gurl.value().spec().c_str());
  }

  bool VerifyCacheClipboardContentTypeExists(ClipboardContentType type) {
    std::optional<std::set<ClipboardContentType>> cached_content_types =
        clipboard_content_->GetCachedClipboardContentTypes();
    if (cached_content_types.has_value()) {
      return cached_content_types.value().find(type) !=
             cached_content_types.value().end();
    } else {
      return false;
    }
  }

  void VerifiyClipboardURLIsInvalid() {
    VerifyClipboardTypeExists(ClipboardContentType::URL, true);

    __block BOOL callback_called = NO;
    __block std::optional<GURL> optional_gurl;
    clipboard_content_->GetRecentURLFromClipboard(
        base::BindOnce(^(std::optional<GURL> copied_url) {
          optional_gurl = copied_url;
          callback_called = YES;
        }));
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForCookiesTimeout, ^bool {
      base::RunLoop().RunUntilIdle();
      return callback_called;
    }));
    EXPECT_FALSE(optional_gurl.has_value());
  }

  bool WaitForClipboardContentTypesRefresh() {
    bool success = WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool() {
      return clipboard_content_->GetCachedClipboardContentTypes().has_value();
    });

    return success;
  }
};

TEST_F(ClipboardRecentContentIOSTest, SchemeFiltering) {
  // Test unrecognized URL.
  SetPasteboardContent(kUnrecognizedURL);
  VerifiyClipboardURLIsInvalid();

  // Test recognized URL.
  SetPasteboardContent(kRecognizedURL);
  VerifyClipboardURLExists(kRecognizedURL);

  // Test URL with app specific scheme.
  SetPasteboardContent(kAppSpecificURL);
  VerifyClipboardURLExists(kAppSpecificURL);

  // Test URL without app specific scheme.
  ResetClipboardRecentContent(std::string(), base::Days(10));

  SetPasteboardContent(kAppSpecificURL);
  VerifiyClipboardURLIsInvalid();
}

TEST_F(ClipboardRecentContentIOSTest, PasteboardURLObsolescence) {
  SetPasteboardContent(kRecognizedURL);

  // Test that recent pasteboard data is provided.
  VerifyClipboardURLExists(kRecognizedURL);

  // Test that old pasteboard data is not provided.
  SetStoredPasteboardChangeDate(
      [NSDate dateWithTimeIntervalSinceNow:-kLongerThanMaxAge]);

  VerifyClipboardTypeExists(ClipboardContentType::URL, false);
  VerifyClipboardTypeExists(ClipboardContentType::Text, false);

  // Tests that if chrome is relaunched, old pasteboard data is still
  // not provided.
  ResetClipboardRecentContent(kAppSpecificScheme, base::Days(10));
  VerifyClipboardTypeExists(ClipboardContentType::URL, false);
  VerifyClipboardTypeExists(ClipboardContentType::Text, false);

  SimulateDeviceRestart();
  // Tests that if the device is restarted, old pasteboard data is still
  // not provided.
  VerifyClipboardTypeExists(ClipboardContentType::URL, false);
  VerifyClipboardTypeExists(ClipboardContentType::Text, false);
}

TEST_F(ClipboardRecentContentIOSTest,
       CacheClipboardContentTypesUpdatesForCopiedURL) {
  SetPasteboardContent(kRecognizedURL);
  ASSERT_TRUE(WaitForClipboardContentTypesRefresh());

  EXPECT_TRUE(VerifyCacheClipboardContentTypeExists(ClipboardContentType::URL));
  EXPECT_FALSE(
      VerifyCacheClipboardContentTypeExists(ClipboardContentType::Image));
  EXPECT_FALSE(
      VerifyCacheClipboardContentTypeExists(ClipboardContentType::Text));
}

TEST_F(ClipboardRecentContentIOSTest,
       CacheClipboardContentTypesUpdatesForCopiedImage) {
  SetPasteboardImage(TestUIImage());
  ASSERT_TRUE(WaitForClipboardContentTypesRefresh());

  EXPECT_TRUE(
      VerifyCacheClipboardContentTypeExists(ClipboardContentType::Image));
  EXPECT_FALSE(
      VerifyCacheClipboardContentTypeExists(ClipboardContentType::URL));
  EXPECT_FALSE(
      VerifyCacheClipboardContentTypeExists(ClipboardContentType::Text));
}

TEST_F(ClipboardRecentContentIOSTest,
       CacheClipboardContentTypesUpdatesForCopiedText) {
  SetPasteboardContent("foobar");
  ASSERT_TRUE(WaitForClipboardContentTypesRefresh());

  EXPECT_TRUE(
      VerifyCacheClipboardContentTypeExists(ClipboardContentType::Text));
  EXPECT_FALSE(
      VerifyCacheClipboardContentTypeExists(ClipboardContentType::Image));
  EXPECT_FALSE(
      VerifyCacheClipboardContentTypeExists(ClipboardContentType::URL));
}

// Checks that if the pasteboard is marked as having confidential data, it is
// not returned.
TEST_F(ClipboardRecentContentIOSTest, ConfidentialPasteboardText) {
  [[UIPasteboard generalPasteboard]
      setItems:@[ @{
        @"public.plain-text" : @"hunter2",
        @"org.nspasteboard.ConcealedType" : @"hunter2"
      } ]
       options:@{}];

  VerifyClipboardTypeExists(ClipboardContentType::Text, false);
}

// Checks that if the user suppresses content, no text will be returned,
// and if the text changes, the new text will be returned again.
TEST_F(ClipboardRecentContentIOSTest, SuppressedPasteboardContent) {
  SetPasteboardContent(kRecognizedURL);

  // Test that recent pasteboard data is provided.
  VerifyClipboardURLExists(kRecognizedURL);

  // Suppress the content of the pasteboard.
  clipboard_content_->SuppressClipboardContent();

  // Check that the pasteboard content is suppressed.
  VerifyClipboardTypeExists(ClipboardContentType::URL, false);

  // Create a new clipboard content to test persistence.
  ResetClipboardRecentContent(kAppSpecificScheme, base::Days(10));

  // Check that the pasteboard content is still suppressed.
  VerifyClipboardTypeExists(ClipboardContentType::URL, false);

  // Check that even if the device is restarted, pasteboard content is
  // still suppressed.
  SimulateDeviceRestart();
  VerifyClipboardTypeExists(ClipboardContentType::URL, false);

  // Check that if the pasteboard changes, the new content is not
  // suppressed anymore.
  SetPasteboardContent(kRecognizedURL2);
  VerifyClipboardURLExists(kRecognizedURL2);
}

// TODO(crbug.com/40275048): This test is flaky.
// Checks that if the user suppresses content, no image will be returned,
// and if the image changes, the new image will be returned again.
TEST_F(ClipboardRecentContentIOSTest, DISABLED_SuppressedPasteboardImage) {
  SetPasteboardImage(TestUIImage());

  // Test that recent pasteboard data is provided.
  VerifyClipboardTypeExists(ClipboardContentType::Image, true);

  // Suppress the content of the pasteboard.
  clipboard_content_->SuppressClipboardContent();

  // Check that the pasteboard content is suppressed.
  VerifyClipboardTypeExists(ClipboardContentType::Image, false);

  // Create a new clipboard content to test persistence.
  ResetClipboardRecentContent(kAppSpecificScheme, base::Days(10));

  // Check that the pasteboard content is still suppressed.
  VerifyClipboardTypeExists(ClipboardContentType::Image, false);

  // Check that even if the device is restarted, pasteboard content is
  // still suppressed.
  SimulateDeviceRestart();
  VerifyClipboardTypeExists(ClipboardContentType::Image, false);

  // Check that if the pasteboard changes, the new content is not
  // suppressed anymore.
  SetPasteboardImage(TestUIImage([UIColor greenColor]));
  VerifyClipboardTypeExists(ClipboardContentType::Image, true);
}

// TODO(crbug.com/40275048): This test is flaky.
// Checks that if user copies something other than a string we don't cache the
// string in pasteboard.
TEST_F(ClipboardRecentContentIOSTest,
       DISABLED_AddingNonStringRemovesCachedString) {
  SetPasteboardContent(kRecognizedURL);

  // Test that recent pasteboard data is provided as url.
  VerifyClipboardURLExists(kRecognizedURL);
  VerifyClipboardTypeExists(ClipboardContentType::Text, false);
  // Image pasteboard should be empty.
  VerifyClipboardTypeExists(ClipboardContentType::Image, false);

  // Overwrite pasteboard with an image.
  SetPasteboardImage(TestUIImage());

  // Url and text pasteboard should appear empty.
  VerifyClipboardTypeExists(ClipboardContentType::URL, false);
  VerifyClipboardTypeExists(ClipboardContentType::Text, false);
  // Image pasteboard should be full
  VerifyClipboardTypeExists(ClipboardContentType::Image, true);

  // Tests that if URL is added again, pasteboard provides it normally.
  SetPasteboardContent(kRecognizedURL);
  VerifyClipboardURLExists(kRecognizedURL);
  VerifyClipboardTypeExists(ClipboardContentType::Text, false);
  // Image pasteboard should be empty.
  VerifyClipboardTypeExists(ClipboardContentType::Image, false);
}
