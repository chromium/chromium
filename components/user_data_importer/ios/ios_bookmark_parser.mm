// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/user_data_importer/ios/ios_bookmark_parser.h"

#import <WebKit/WebKit.h>

#import "base/apple/foundation_util.h"
#import "base/functional/callback_helpers.h"
#import "base/json/json_reader.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "base/threading/sequence_bound.h"
#import "base/types/expected_macros.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/web_view_js_utils.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
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

using JSONOrErrorCallback = base::OnceCallback<void(std::string, NSError*)>;

namespace {

// Turns a list representing a path to a bookmark/folder from a JSON-source
// base::DictValue into a vector of strings. Moves path components out of
// `value` (i.e., `value` is no longer valid after this operation). Returns
// nullopt if the list is malformed or nullptr.
std::optional<std::vector<std::u16string>> PathListFromValue(
    base::ListValue* value) {
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

// Transforms a JSON-source base::DictValue representing a single bookmark into
// an ImportedBookmarkEntry. Returns nullopt if the base::DictValue is invalid.
std::optional<ImportedBookmarkEntry> BookmarkEntryFromValue(
    base::DictValue dict) {
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

std::vector<ImportedBookmarkEntry> TransformList(base::ListValue list) {
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
BookmarkParser::BookmarkParsingResult TranslateJSResult(base::Value value) {
  if (!value.is_dict()) {
    return base::unexpected(
        BookmarkParser::BookmarkParsingError::kParsingFailed);
  }
  base::DictValue dict = std::move(value).TakeDict();

  BookmarkParser::ParsedBookmarks parsing_result;

  if (base::ListValue* bookmarks = dict.FindList("bookmarks")) {
    parsing_result.bookmarks = TransformList(std::move(*bookmarks));
  } else {
    return base::unexpected(
        BookmarkParser::BookmarkParsingError::kParsingFailed);
  }

  // Note: Although Reading List is often omitted from input files, the TS
  // parser is still expected to return an empty list in these cases. Thus, the
  // field is required.
  if (base::ListValue* reading_list = dict.FindList("readingList")) {
    parsing_result.reading_list = TransformList(std::move(*reading_list));
  } else {
    return base::unexpected(
        BookmarkParser::BookmarkParsingError::kParsingFailed);
  }

  return base::ok(std::move(parsing_result));
}

}  // namespace

// Helper class that encapsulates the parts of the import flow that interact
// with WKWebView and thus must run on the main thread. Because this is on the
// main thread, it should be kept to the minimal necessary set of work; in
// particular, expensive work like file I/O or parsing belongs in the main
// class (which is safe to use on non-main sequences).
class WebViewRunner {
 public:
  WebViewRunner() {
    web_view_ =
        [[WKWebView alloc] initWithFrame:CGRectZero
                           configuration:[[WKWebViewConfiguration alloc] init]];
  }

  ~WebViewRunner() { web_view_.navigationDelegate = nil; }

  // Parses the given `file` using the given `script`. Executes `callback` with
  // the returned JSON or an NSError if execution fails.
  void LoadAndParse(base::FilePath file,
                    std::string script,
                    JSONOrErrorCallback callback) {
    NSURL* url = base::apple::FilePathToNSURL(file);
    NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:url];
    [request addValue:@"text/html; charset=utf-8"
        forHTTPHeaderField:@"Content-Type"];

    // Configure the WKWebView so that the parsing JS is injected and run once
    // the content has loaded.
    base::OnceClosure on_load = base::BindOnce(
        &WebViewRunner::TriggerParseInJS, weak_factory_.GetWeakPtr(),
        std::move(script), std::move(callback));
    forwarder_ =
        [[LocalNavigationForwarder alloc] initWithClosure:std::move(on_load)];
    web_view_.navigationDelegate = forwarder_;

    // Passing `url` as the second parameter prevents any resources other than
    // `url` from being opened (e.g. in iframes). This is a security mitigation,
    // so don't change it unless you're sure you're doing the right thing.
    [web_view_ loadFileRequest:request allowingReadAccessToURL:url];
  }

 private:
  // Executes the given `script` in `web_view_`. Should be run only once the
  // target file has finished loading in `web_view_`.
  void TriggerParseInJS(std::string script, JSONOrErrorCallback callback) {
    NSString* ns_script = base::SysUTF8ToNSString(script);

    [web_view_ callAsyncJavaScript:ns_script
                         arguments:nil
                           inFrame:nil
                    inContentWorld:WKContentWorld.defaultClientWorld
                 completionHandler:base::CallbackToBlock(base::BindOnce(
                                       &WebViewRunner::OnJSResult,
                                       weak_factory_.GetWeakPtr(),
                                       std::move(callback)))];
  }

  // Triggered once JS has completed parsing of the target file.
  void OnJSResult(JSONOrErrorCallback callback, id result, NSError* error) {
    std::string result_str;
    if (!error && [result isKindOfClass:[NSString class]]) {
      result_str = base::SysNSStringToUTF8(result);
    }
    std::move(callback).Run(std::move(result_str), error);
  }

  WKWebView* web_view_;
  LocalNavigationForwarder* forwarder_;
  base::WeakPtrFactory<WebViewRunner> weak_factory_{this};
};

// Declared in bookmark_parser.h.
std::unique_ptr<BookmarkParser> MakeBookmarkParser() {
  return std::make_unique<IOSBookmarkParser>();
}

IOSBookmarkParser::IOSBookmarkParser()
    : runner_(web::GetUIThreadTaskRunner({})) {}
IOSBookmarkParser::~IOSBookmarkParser() = default;

void IOSBookmarkParser::Parse(
    const base::FilePath& file,
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

  std::string script_str = base::SysNSStringToUTF8(script);

  runner_.AsyncCall(&WebViewRunner::LoadAndParse)
      .WithArgs(file, std::move(script_str),
                base::BindPostTaskToCurrentDefault(base::BindOnce(
                    &IOSBookmarkParser::OnJSResult, weak_factory_.GetWeakPtr(),
                    std::move(callback))));
}

void IOSBookmarkParser::OnJSResult(
    BookmarkParser::BookmarkParsingCallback callback,
    std::string result,
    NSError* error) {
  if (error) {
    std::move(callback).Run(
        base::unexpected(BookmarkParser::BookmarkParsingError::kParsingFailed));
    return;
  }

  std::optional<base::Value> value =
      base::JSONReader::Read(result, base::JSON_PARSE_RFC);
  if (!value || !value->is_dict()) {
    std::move(callback).Run(
        base::unexpected(BookmarkParser::BookmarkParsingError::kParsingFailed));
    return;
  }

  BookmarkParsingResult parsing_result = TranslateJSResult(std::move(*value));

  std::move(callback).Run(std::move(parsing_result));
}

}  // namespace user_data_importer
