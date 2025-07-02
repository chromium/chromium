// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/user_data_importer/ios/ios_bookmark_parser.h"

#import <WebKit/WebKit.h>

#import "base/apple/foundation_util.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/utf_string_conversions.h"
#import "base/types/expected_macros.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/web_view_js_utils.h"
#import "url/gurl.h"

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

// Turns a list representing a path to a bookmark/folder from a JSON-source
// Value::Dict into a vector of strings. Moves path components out of `value`
// (i.e., `value` is no longer valid after this operation). Returns nullopt if
// the list is malformed or nullptr.
std::optional<std::vector<std::u16string>> PathListFromValue(
    base::Value::List* value) {
  if (!value) {
    return std::nullopt;
  }
  std::vector<std::u16string> result;
  for (base::Value& subvalue : *value) {
    if (subvalue.is_string()) {
      result.push_back(base::UTF8ToUTF16(std::move(subvalue).TakeString()));
    } else {
      return std::nullopt;
    }
  }
  return result;
}

// Workaround to avoid implicit conversion to optional in error case.
std::optional<ImportedBookmarkEntry> NullEntry() {
  return std::nullopt;
}

// Transforms a JSON-source Value::Dict representing a single bookmark into an
// ImportedBookmarkEntry. Returns nullopt if the Value::Dict is invalid.
std::optional<ImportedBookmarkEntry> BookmarkEntryFromValue(
    base::Value::Dict dict) {
  ImportedBookmarkEntry entry;
  entry.in_toolbar = false;  // Not supported.

  // `isFolder` and `path` are always mandatory.
  ASSIGN_OR_RETURN(entry.is_folder, dict.FindBool("isFolder"), &NullEntry);
  ASSIGN_OR_RETURN(entry.path, PathListFromValue(dict.FindList("path")),
                   &NullEntry);

  // `url` is required for non-folder items.
  if (!entry.is_folder) {
    if (std::string* url = dict.FindString("url")) {
      entry.url = GURL(*url);
    } else {
      return std::nullopt;
    }
  }

  // `title` is optional. If no valid title is found, the default ("") is OK.
  if (std::string* title = dict.FindString("title")) {
    entry.title = base::UTF8ToUTF16(*title);
  }

  // `creationTime` is also optional. If not found, set to the current time, as
  // the default-constructed time is nonsensical.
  if (std::optional<double> creation_time = dict.FindDouble("creationTime")) {
    entry.creation_time = base::Time::FromSecondsSinceUnixEpoch(*creation_time);
  } else {
    entry.creation_time = base::Time::Now();
  }

  return entry;
}

std::vector<ImportedBookmarkEntry> TransformList(base::Value::List list) {
  std::vector<ImportedBookmarkEntry> result;
  for (base::Value& val : list) {
    if (!val.is_dict()) {
      continue;
    }
    std::optional<ImportedBookmarkEntry> entry =
        BookmarkEntryFromValue(std::move(val).TakeDict());
    if (entry) {
      result.push_back(std::move(*entry));
    }
  }
  return result;
}

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
  if (!value_result || !value_result->is_dict()) {
    return base::unexpected(
        BookmarkParser::BookmarkParsingError::kParsingFailed);
  }
  base::Value::Dict dict = std::move(*value_result).TakeDict();

  BookmarkParser::ParsedBookmarks parsing_result;

  if (base::Value::List* bookmarks = dict.FindList("bookmarks")) {
    parsing_result.bookmarks = TransformList(std::move(*bookmarks));
  } else {
    return base::unexpected(
        BookmarkParser::BookmarkParsingError::kParsingFailed);
  }

  // Note: Although Reading List is often omitted from input files, the TS
  // parser is still expected to return an empty list in these cases. Thus, the
  // field is required.
  if (base::Value::List* reading_list = dict.FindList("readingList")) {
    parsing_result.reading_list = TransformList(std::move(*reading_list));
  } else {
    return base::unexpected(
        BookmarkParser::BookmarkParsingError::kParsingFailed);
  }

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
  NSString* path = [NSBundle.mainBundle pathForResource:@"bookmark_parser"
                                                 ofType:@"js"];
  NSError* error = nil;
  NSString* script = [NSString stringWithContentsOfFile:path
                                               encoding:NSUTF8StringEncoding
                                                  error:&error];
  CHECK(!error);

  // See comment at bottom of `bookmark_parser.ts` for context on this magic
  // string.
  script = [script stringByAppendingString:@"\nreturn parsed;"];

  // TODO: Add an appropriate timeout
  [web_view_ callAsyncJavaScript:script
                       arguments:nil
                         inFrame:nil
                  inContentWorld:WKContentWorld.defaultClientWorld
               completionHandler:base::CallbackToBlock(
                                     base::BindOnce(&TranslateJSResult)
                                         .Then(std::move(callback)))];
}

}  // namespace user_data_importer
