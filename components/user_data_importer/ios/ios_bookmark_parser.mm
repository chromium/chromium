// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/user_data_importer/ios/ios_bookmark_parser.h"

#import <WebKit/WebKit.h>

#import "base/apple/foundation_util.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/web_view_js_utils.h"

// Object that conforms to WKNavigationDelegate and runs a provided OnceClosure
// the first time a webView:didFinishNavigation: message is received.
@interface LocalNavigationForwarder : NSObject <WKNavigationDelegate> {
  base::OnceClosure _triggerOnLoad;
}

// Initializes the forwarder and configures it to run the given `closure`.
- (instancetype)initWithClosure:(base::OnceClosure)closure;

@end

@implementation LocalNavigationForwarder
#pragma mark - WKNavigationDelegate

- (instancetype)initWithClosure:(base::OnceClosure)closure {
  self = [super init];
  if (self) {
    _triggerOnLoad = std::move(closure);
  }
  return self;
}

- (void)webView:(WKWebView*)webView
    didFinishNavigation:(WKNavigation*)navigation {
  if (_triggerOnLoad) {
    std::move(_triggerOnLoad).Run();
  }
}

- (void)webView:(WKWebView*)webView
    didFailNavigation:(WKNavigation*)navigation
            withError:(NSError*)error {
  // This is a local file created by the browser prior to invoking this logic,
  // so if we fail navigation, it implies a bug in the configuration of this
  // flow. Implementing this delegate method and firing a NOTREACHED prevents
  // us from failing silently and hanging indefinitely, should such a bug ever
  // be introduced.
  NOTREACHED();
}

- (void)webView:(WKWebView*)webView
    didFailProvisionalNavigation:(WKNavigation*)navigation
                       withError:(NSError*)error {
  // This is a local file created by the browser prior to invoking this logic,
  // so if we fail navigation, it implies a bug in the configuration of this
  // flow. Implementing this delegate method and firing a NOTREACHED prevents
  // us from failing silently and hanging indefinitely, should such a bug ever
  // be introduced.
  NOTREACHED();
}
@end

namespace user_data_importer {

namespace {

// Transforms the result or error of the JS call into a result or error suitable
// for invoking a BookmarkParsingCallback.
BookmarkParser::BookmarkParsingResult TranslateJSResult(id result,
                                                        NSError* error) {
  if (error) {
    return base::unexpected(
        BookmarkParser::BookmarkParsingError::kParsingFailed);
  }
  std::unique_ptr<base::Value> value_result =
      web::ValueResultFromWKResult(result);
  if (!value_result || !value_result->is_string()) {
    return base::unexpected(
        BookmarkParser::BookmarkParsingError::kParsingFailed);
  }

  // TODO(crbug.com/407587751): This is a placeholder; transform this into
  // meaningful data.
  ImportedBookmarkEntry placeholder_entry;
  placeholder_entry.title = base::UTF8ToUTF16(value_result->GetString());

  BookmarkParser::ParsedBookmarks parsing_result;
  parsing_result.bookmarks = {std::move(placeholder_entry)};

  return base::ok(std::move(parsing_result));
}

}  // namespace

// Declared in bookmark_parser.h.
std::unique_ptr<BookmarkParser> MakeBookmarkParser() {
  return std::make_unique<IOSBookmarkParser>();
}

IOSBookmarkParser::IOSBookmarkParser() {
  web_view_ =
      [[WKWebView alloc] initWithFrame:CGRectZero
                         configuration:[[WKWebViewConfiguration alloc] init]];
}
IOSBookmarkParser::~IOSBookmarkParser() = default;

void IOSBookmarkParser::Parse(
    const base::FilePath& file,
    BookmarkParser::BookmarkParsingCallback callback) {
  if (!web_view_) {
    std::move(callback).Run(
        base::unexpected(BookmarkParser::BookmarkParsingError::kOther));
    return;
  }
  NSURL* url = base::apple::FilePathToNSURL(file);
  CHECK(url);

  NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:url];
  [request addValue:@"text/html; charset=utf-8"
      forHTTPHeaderField:@"Content-Type"];

  // Configure the WKWebView so that the parsing JS is injected and run once the
  // content has loaded.
  forwarder_ = [[LocalNavigationForwarder alloc]
      initWithClosure:base::BindOnce(&IOSBookmarkParser::TriggerParseInJS,
                                     weak_factory_.GetWeakPtr(),
                                     std::move(callback))];
  web_view_.navigationDelegate = forwarder_;

  // Passing `url` as the second parameter prevents any resources other than
  // `url` from being opened (e.g. in iframes). This is a security mitigation,
  // so don't change it unless you're sure you're doing the right thing.
  [web_view_ loadFileRequest:request allowingReadAccessToURL:url];
}

void IOSBookmarkParser::TriggerParseInJS(
    BookmarkParser::BookmarkParsingCallback callback) {
  // TODO(crbug.com/407587751): Replace this with an actual parser.
  NSString* script = @"return document.title;";
  [web_view_ callAsyncJavaScript:script
                       arguments:nil
                         inFrame:nil
                  inContentWorld:WKContentWorld.defaultClientWorld
               completionHandler:base::CallbackToBlock(
                                     base::BindOnce(&TranslateJSResult)
                                         .Then(std::move(callback)))];
}

}  // namespace user_data_importer
