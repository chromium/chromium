// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/open_from_clipboard/clipboard_recent_content_impl_ios.h"

#import <MobileCoreServices/MobileCoreServices.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"
#include "components/open_from_clipboard/clipboard_async_wrapper_ios.h"

ContentType const ContentTypeURL = @"ContentTypeURL";
ContentType const ContentTypeText = @"ContentTypeString";
ContentType const ContentTypeImage = @"ContentTypeImage";

namespace {
// Key used to store the pasteboard's current change count. If when resuming
// chrome the pasteboard's change count is different from the stored one, then
// it means that the pasteboard's content has changed.
NSString* const kPasteboardChangeCountKey = @"PasteboardChangeCount";
// Key used to store the last date at which it was detected that the pasteboard
// changed. It is used to evaluate the age of the pasteboard's content.
NSString* const kPasteboardChangeDateKey = @"PasteboardChangeDate";
// Default Scheme to use for urls with no scheme.
NSString* const kDefaultScheme = @"https";

}  // namespace

@interface ClipboardRecentContentImplIOS ()

// The user defaults from the app group used to optimize the pasteboard change
// detection.
@property(nonatomic, strong) NSUserDefaults* sharedUserDefaults;
// The pasteboard's change count. Increases everytime the pasteboard changes.
@property(nonatomic) NSInteger lastPasteboardChangeCount;
// Contains the authorized schemes for URLs.
@property(nonatomic, readonly) NSSet* authorizedSchemes;
// Delegate for metrics.
@property(nonatomic, strong) id<ClipboardRecentContentDelegate> delegate;
// Maximum age of clipboard in seconds.
@property(nonatomic, readonly) NSTimeInterval maximumAgeOfClipboard;
// Whether the clipboard should only be accessed asynchronously.
@property(nonatomic, assign) BOOL onlyUseClipboardAsync;

// A cached version of an already-retrieved URL. This prevents subsequent URL
// requests from triggering the iOS 14 pasteboard notification.
@property(nonatomic, strong) NSURL* cachedURL;
// A cached version of an already-retrieved string. This prevents subsequent
// string requests from triggering the iOS 14 pasteboard notification.
@property(nonatomic, copy) NSString* cachedText;
// A cached version of an already-retrieved image. This prevents subsequent
// image requests from triggering the iOS 14 pasteboard notification.
@property(nonatomic, strong) UIImage* cachedImage;
// A cached set of the content types currently being used for the clipboard.
@property(nonatomic, strong) NSSet<ContentType>* cachedContentTypes;

// Loads information from the user defaults about the latest pasteboard entry.
- (void)loadFromUserDefaults;

// Returns the URL contained in `pasteboard` (if any).
- (NSURL*)URLFromPasteboard:(UIPasteboard*)pasteboard;

// Returns the uptime.
- (NSTimeInterval)uptime;

// Calls |completionHandler| with the result of whether or not the clipboard
// currently contains data matching |contentType|.
- (void)checkForContentType:(ContentType)contentType
                 pasteboard:(UIPasteboard*)pasteboard
          completionHandler:(void (^)(BOOL))completionHandler;

// Checks the clipboard for content matching |types| and calls
// |completionHandler| once all types are checked. This method is called
// recursively and partial results are passed in |results| until all types have
// been checked.
- (void)
    hasContentMatchingRemainingTypes:(NSSet<ContentType>*)types
                          pasteboard:(UIPasteboard*)pasteboard
                             results:
                                 (NSMutableDictionary<ContentType, NSNumber*>*)
                                     results
                   completionHandler:
                       (void (^)(NSSet<ContentType>*))completionHandler;

@end

@implementation ClipboardRecentContentImplIOS

- (instancetype)initWithMaxAge:(NSTimeInterval)maxAge
             authorizedSchemes:(NSSet<NSString*>*)authorizedSchemes
                  userDefaults:(NSUserDefaults*)groupUserDefaults
         onlyUseClipboardAsync:(BOOL)onlyUseClipboardAsync
                      delegate:(id<ClipboardRecentContentDelegate>)delegate {
  self = [super init];
  if (self) {
    _maximumAgeOfClipboard = maxAge;
    _delegate = delegate;
    _authorizedSchemes = authorizedSchemes;
    _sharedUserDefaults = groupUserDefaults;
    _onlyUseClipboardAsync = onlyUseClipboardAsync;

    _lastPasteboardChangeCount = NSIntegerMax;
    [self loadFromUserDefaults];
    [self updateCachedClipboardState];

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(didBecomeActive:)
               name:UIApplicationDidBecomeActiveNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(pasteboardDidChange:)
               name:UIPasteboardChangedNotification
             object:nil];

    __weak __typeof(self) weakSelf = self;
    GetGeneralPasteboard(
        _onlyUseClipboardAsync, base::BindOnce(^(UIPasteboard* pasteboard) {
          [weakSelf updateIfNeededWithPasteboard:pasteboard];
          // Makes sure |last_pasteboard_change_count_| was properly
          // initialized.
          DCHECK_NE(weakSelf.lastPasteboardChangeCount, NSIntegerMax);
        }));
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)didBecomeActive:(NSNotification*)notification {
  [self loadFromUserDefaults];
  [self updateCachedClipboardState];

  __weak __typeof(self) weakSelf = self;
  GetGeneralPasteboard(self.onlyUseClipboardAsync,
                       base::BindOnce(^(UIPasteboard* pasteboard) {
                         [weakSelf updateIfNeededWithPasteboard:pasteboard];
                       }));
}

#pragma mark - Public

- (NSURL*)recentURLFromClipboard {
  // If the clipboard can only be accessed asynchronously, then this method
  // cannot even check whether the existing cached URL is stil valid.
  if (self.onlyUseClipboardAsync) {
    return nil;
  }
  return [self recentURLFromPasteboard:UIPasteboard.generalPasteboard];
}

- (NSString*)recentTextFromClipboard {
  // If the clipboard can only be accessed asynchronously, then this method
  // cannot even check whether the existing cached text is stil valid.
  if (self.onlyUseClipboardAsync) {
    return nil;
  }

  return [self recentTextFromPasteboard:UIPasteboard.generalPasteboard];
}

- (UIImage*)recentImageFromClipboard {
  // If the clipboard can only be accessed asynchronously, then this method
  // cannot even check whether the existing cached image is stil valid.
  if (self.onlyUseClipboardAsync) {
    return nil;
  }

  return [self recentImageFromPasteboard:UIPasteboard.generalPasteboard];
}

- (NSSet<ContentType>*)cachedClipboardContentTypes {
  if (![self shouldReturnValueOfClipboard:nil]) {
    return nil;
  }
  return self.cachedContentTypes;
}

- (void)hasContentMatchingTypes:(NSSet<ContentType>*)types
              completionHandler:
                  (void (^)(NSSet<ContentType>*))completionHandler {
  __weak __typeof(self) weakSelf = self;
  GetGeneralPasteboard(self.onlyUseClipboardAsync,
                       base::BindOnce(^(UIPasteboard* pasteboard) {
                         [weakSelf hasContentMatchingTypes:types
                                                pasteboard:pasteboard
                                         completionHandler:completionHandler];
                       }));
}

- (void)recentURLFromClipboardAsync:(void (^)(NSURL*))callback {
  __weak __typeof(self) weakSelf = self;
  GetGeneralPasteboard(
      self.onlyUseClipboardAsync, base::BindOnce(^(UIPasteboard* pasteboard) {
        [weakSelf recentURLFromClipboardAsyncWithPasteboard:pasteboard
                                                   callback:callback];
      }));
}

- (void)recentTextFromClipboardAsync:(void (^)(NSString*))callback {
  __weak __typeof(self) weakSelf = self;
  GetGeneralPasteboard(
      self.onlyUseClipboardAsync, base::BindOnce(^(UIPasteboard* pasteboard) {
        [weakSelf recentTextFromClipboardAsyncWithPasteboard:pasteboard
                                                    callback:callback];
      }));
}

- (void)recentImageFromClipboardAsync:(void (^)(UIImage*))callback {
  __weak __typeof(self) weakSelf = self;
  GetGeneralPasteboard(
      self.onlyUseClipboardAsync, base::BindOnce(^(UIPasteboard* pasteboard) {
        [weakSelf recentImageFromClipboardAsyncWithPasteboard:pasteboard
                                                     callback:callback];
      }));
}

- (NSTimeInterval)clipboardContentAge {
  return -[self.lastPasteboardChangeDate timeIntervalSinceNow];
}

- (void)suppressClipboardContent {
  // User cleared the user data. The pasteboard entry must be removed from the
  // omnibox list. Force entry expiration by setting copy date to 1970.
  self.lastPasteboardChangeDate =
      [[NSDate alloc] initWithTimeIntervalSince1970:0];
  [self saveToUserDefaults];
}

- (void)saveToUserDefaults {
  [self.sharedUserDefaults setInteger:self.lastPasteboardChangeCount
                               forKey:kPasteboardChangeCountKey];
  [self.sharedUserDefaults setObject:self.lastPasteboardChangeDate
                              forKey:kPasteboardChangeDateKey];
}

#pragma mark - Private

// Returns whether the pasteboard changed since the last time a pasteboard
// change was detected.
- (BOOL)hasPasteboardChanged:(UIPasteboard*)pasteboard {
  return pasteboard.changeCount != self.lastPasteboardChangeCount;
}

- (void)pasteboardDidChange:(NSNotification*)notification {
  [self updateCachedClipboardState];
}

- (void)updateCachedClipboardState {
  self.cachedContentTypes = nil;

  NSSet<ContentType>* desiredContentTypes = [NSSet
      setWithArray:@[ ContentTypeImage, ContentTypeURL, ContentTypeText ]];
  __weak __typeof(self) weakSelf = self;
  [self hasContentMatchingTypes:desiredContentTypes
              completionHandler:^(NSSet<ContentType>* results) {
                weakSelf.cachedContentTypes = results;
              }];
}

// The synchronous version of this method is kept around for public APIs and old
// iOS versions.
- (NSURL*)recentURLFromPasteboard:(UIPasteboard*)pasteboard {
  [self updateIfNeededWithPasteboard:pasteboard];

  if (![self shouldReturnValueOfClipboard:pasteboard]) {
    return nil;
  }

  return self.cachedURL;
}

// The synchronous version of this method is kept around for public APIs and old
// iOS versions.
- (NSString*)recentTextFromPasteboard:(UIPasteboard*)pasteboard {
  [self updateIfNeededWithPasteboard:pasteboard];

  if (![self shouldReturnValueOfClipboard:pasteboard]) {
    return nil;
  }

  return self.cachedText;
}

// The synchronous version of this method is kept around for public APIs and old
// iOS versions.
- (UIImage*)recentImageFromPasteboard:(UIPasteboard*)pasteboard {
  [self updateIfNeededWithPasteboard:pasteboard];

  if (![self shouldReturnValueOfClipboard:pasteboard]) {
    return nil;
  }

  return self.cachedImage;
}

// Checks for if the given `pasteboard` has content matching the provided
// `types`.
- (void)hasContentMatchingTypes:(NSSet<ContentType>*)types
                     pasteboard:(UIPasteboard*)pasteboard
              completionHandler:
                  (void (^)(NSSet<ContentType>*))completionHandler {
  [self updateIfNeededWithPasteboard:pasteboard];
  if (![self shouldReturnValueOfClipboard:pasteboard] || ![types count]) {
    completionHandler([NSSet set]);
    return;
  }

  [self hasContentMatchingRemainingTypes:types
                              pasteboard:pasteboard
                                 results:[[NSMutableDictionary alloc] init]
                       completionHandler:completionHandler];
}

- (void)
    hasContentMatchingRemainingTypes:(NSSet<ContentType>*)types
                          pasteboard:(UIPasteboard*)pasteboard
                             results:
                                 (NSMutableDictionary<ContentType, NSNumber*>*)
                                     results
                   completionHandler:
                       (void (^)(NSSet<ContentType>*))completionHandler {
  if ([types count] == 0) {
    NSMutableSet<ContentType>* matchingTypes = [NSMutableSet set];
    for (ContentType type in results) {
      if ([results[type] boolValue]) {
        [matchingTypes addObject:type];
      }
    }
    completionHandler(matchingTypes);
    return;
  }

  __weak __typeof(self) weakSelf = self;
  ContentType type = [types anyObject];
  [self checkForContentType:type
                 pasteboard:pasteboard
          completionHandler:^(BOOL hasType) {
            results[type] = @(hasType);

            NSMutableSet* remainingTypes = [types mutableCopy];
            [remainingTypes removeObject:type];
            [weakSelf hasContentMatchingRemainingTypes:remainingTypes
                                            pasteboard:pasteboard
                                               results:results
                                     completionHandler:completionHandler];
          }];
}

- (void)checkForContentType:(ContentType)contentType
                 pasteboard:(UIPasteboard*)pasteboard
          completionHandler:(void (^)(BOOL))completionHandler {
  if ([contentType isEqualToString:ContentTypeText]) {
    [self hasRecentTextFromClipboardInternalWithPasteboard:pasteboard
                                                  callback:^(BOOL hasText) {
                                                    completionHandler(hasText);
                                                  }];
  } else if ([contentType isEqualToString:ContentTypeURL]) {
    [self hasRecentURLFromClipboardInternalWithPasteboard:pasteboard
                                                 callback:^(BOOL hasURL) {
                                                   completionHandler(hasURL);
                                                 }];
  } else if ([contentType isEqualToString:ContentTypeImage]) {
    [self
        hasRecentImageFromClipboardInternalWithPasteboard:pasteboard
                                                 callback:^(BOOL hasImage) {
                                                   completionHandler(hasImage);
                                                 }];
  } else {
    NOTREACHED_IN_MIGRATION() << contentType;
  }
}

// The underlying logic to check if the clipboard has a recent URL, with the
// addition of a `pasteboard` parameter to aid in forcing all pasteboard access
// to be async.
- (void)
    hasRecentURLFromClipboardInternalWithPasteboard:(UIPasteboard*)pasteboard
                                           callback:(void (^)(BOOL))callback {
  DCHECK(callback);
  // Use cached value if it exists.
  if (self.cachedURL) {
    callback(YES);
    return;
  }

  NSSet<UIPasteboardDetectionPattern>* urlPattern =
      [NSSet setWithObject:UIPasteboardDetectionPatternProbableWebURL];
  [pasteboard
      detectPatternsForPatterns:urlPattern
              completionHandler:^(NSSet<UIPasteboardDetectionPattern>* patterns,
                                  NSError* error) {
                callback([patterns
                    containsObject:UIPasteboardDetectionPatternProbableWebURL]);
              }];
}

// The underlying logic to check if the clipboard has recent text, with the
// addition of a `pasteboard` parameter to aid in forcing all pasteboard access
// to be async.
- (void)
    hasRecentTextFromClipboardInternalWithPasteboard:(UIPasteboard*)pasteboard
                                            callback:(void (^)(BOOL))callback {
  DCHECK(callback);
  // Use cached value if it exists.
  if (self.cachedText) {
    callback(YES);
    return;
  }

  NSSet<UIPasteboardDetectionPattern>* textPattern =
      [NSSet setWithObject:UIPasteboardDetectionPatternProbableWebSearch];
  [pasteboard
      detectPatternsForPatterns:textPattern
              completionHandler:^(NSSet<UIPasteboardDetectionPattern>* patterns,
                                  NSError* error) {
                callback([patterns
                    containsObject:
                        UIPasteboardDetectionPatternProbableWebSearch]);
              }];
}

// The underlying logic to check if the clipboard has a recent image, with the
// addition of a `pasteboard` parameter to aid in forcing all pasteboard access
// to be async.
- (void)
    hasRecentImageFromClipboardInternalWithPasteboard:(UIPasteboard*)pasteboard
                                             callback:(void (^)(BOOL))callback {
  DCHECK(callback);
  // Use cached value if it exists
  if (self.cachedImage) {
    callback(YES);
    return;
  }

  callback(pasteboard.hasImages);
}

// The underlying logic to check the recent url, with the addition of a
// `pasteboard` parameter to aid in forcing all pasteboard access to be async.
- (void)recentURLFromClipboardAsyncWithPasteboard:(UIPasteboard*)pasteboard
                                         callback:(void (^)(NSURL*))callback {
  DCHECK(callback);
  [self updateIfNeededWithPasteboard:pasteboard];
  if (![self shouldReturnValueOfClipboard:pasteboard]) {
    callback(nil);
    return;
  }

  // Use cached value if it exists.
  if (self.cachedURL) {
    callback(self.cachedURL);
    return;
  }

  __weak __typeof(self) weakSelf = self;
  NSSet<UIPasteboardDetectionPattern>* urlPattern =
      [NSSet setWithObject:UIPasteboardDetectionPatternProbableWebURL];
  [pasteboard detectValuesForPatterns:urlPattern
                    completionHandler:^(
                        NSDictionary<UIPasteboardDetectionPattern, id>* values,
                        NSError* error) {
                      [weakSelf callCompletionHandlerWithValues:values
                                                       callback:callback
                                                          error:error];
                    }];
}

// Helper method for completion handler block to ensure `self` isn't retained.
- (void)callCompletionHandlerWithValues:
            (NSDictionary<UIPasteboardDetectionPattern, id>*)values
                               callback:(void (^)(NSURL*))callback
                                  error:(NSError*)error {
  // On iOS 16, users can deny access to the clipboard.
  if (error) {
    self.cachedURL = nil;
    callback(nil);
    return;
  }
  NSURL* url =
      [NSURL URLWithString:values[UIPasteboardDetectionPatternProbableWebURL]];

  // |detectValuesForPatterns:| will return a url even if the url
  // is missing a scheme. In this case, default to https.
  if (url && url.scheme == nil) {
    NSURLComponents* components = [[NSURLComponents alloc] initWithURL:url
                                               resolvingAgainstBaseURL:NO];
    components.scheme = kDefaultScheme;
    url = components.URL;
  }

  if (![self.authorizedSchemes containsObject:url.scheme]) {
    self.cachedURL = nil;
    callback(nil);
  } else {
    self.cachedURL = url;
    callback(url);
  }
}

// The underlying logic to check the recent text, with the addition of a
// `pasteboard` parameter to aid in forcing all pasteboard access to be async.
- (void)recentTextFromClipboardAsyncWithPasteboard:(UIPasteboard*)pasteboard
                                          callback:
                                              (void (^)(NSString*))callback {
  DCHECK(callback);
  [self updateIfNeededWithPasteboard:pasteboard];
  if (![self shouldReturnValueOfClipboard:pasteboard]) {
    callback(nil);
    return;
  }

  // Use cached value if it exists.
  if (self.cachedText) {
    callback(self.cachedText);
    return;
  }

  __weak __typeof(self) weakSelf = self;
  NSSet<UIPasteboardDetectionPattern>* textPattern =
      [NSSet setWithObject:UIPasteboardDetectionPatternProbableWebSearch];
  [pasteboard detectValuesForPatterns:textPattern
                    completionHandler:^(
                        NSDictionary<UIPasteboardDetectionPattern, id>* values,
                        NSError* error) {
                      NSString* text =
                          values[UIPasteboardDetectionPatternProbableWebSearch];
                      weakSelf.cachedText = text;

                      callback(text);
                    }];
}

// The underlying logic to check the recent image, with the addition of a
// `pasteboard` parameter to aid in forcing all pasteboard access to be async.
- (void)recentImageFromClipboardAsyncWithPasteboard:(UIPasteboard*)pasteboard
                                           callback:
                                               (void (^)(UIImage*))callback {
  DCHECK(callback);
  [self updateIfNeededWithPasteboard:pasteboard];
  if (![self shouldReturnValueOfClipboard:pasteboard]) {
    callback(nil);
    return;
  }

  if (!self.cachedImage) {
    self.cachedImage = pasteboard.image;
  }
  callback(self.cachedImage);
}

// Returns whether the value of the given clipboard should be returned.
- (BOOL)shouldReturnValueOfClipboard:(UIPasteboard*)pasteboard {
  if ([self clipboardContentAge] > self.maximumAgeOfClipboard)
    return NO;

  // It is the common convention on iOS that password managers tag confidential
  // data with the flavor "org.nspasteboard.ConcealedType". Obey this
  // convention; the user doesn't want for their confidential data to be
  // suggested as a search, anyway. See http://nspasteboard.org/ for more info.
  NSArray<NSString*>* types = [pasteboard pasteboardTypes];
  if ([types containsObject:@"org.nspasteboard.ConcealedType"])
    return NO;

  return YES;
}

// If the content of the pasteboard has changed, updates the change count
// and change date.
- (void)updateIfNeededWithPasteboard:(UIPasteboard*)pasteboard {
  if (![self hasPasteboardChanged:pasteboard]) {
    return;
  }

  self.lastPasteboardChangeDate = [NSDate date];
  self.lastPasteboardChangeCount = pasteboard.changeCount;

  // Clear the cache because the pasteboard data has changed.
  self.cachedURL = nil;
  self.cachedText = nil;
  self.cachedImage = nil;

  [self.delegate onClipboardChanged];

  [self saveToUserDefaults];
}

- (NSURL*)URLFromPasteboard:(UIPasteboard*)pasteboard {
  NSURL* url = pasteboard.URL;
  // Usually, even if the user copies plaintext, if it looks like a URL, the URL
  // property is filled. Sometimes, this doesn't happen, for instance when the
  // pasteboard is sync'd from a Mac to the iOS simulator. In this case,
  // fallback and manually check whether the pasteboard contains a url-like
  // string.
  if (!url) {
    url = [NSURL URLWithString:pasteboard.string];
  }
  if (![self.authorizedSchemes containsObject:url.scheme]) {
    return nil;
  }
  return url;
}

- (void)loadFromUserDefaults {
  self.lastPasteboardChangeCount =
      [self.sharedUserDefaults integerForKey:kPasteboardChangeCountKey];
  self.lastPasteboardChangeDate = base::apple::ObjCCastStrict<NSDate>(
      [self.sharedUserDefaults objectForKey:kPasteboardChangeDateKey]);
}

- (NSTimeInterval)uptime {
  return base::SysInfo::Uptime().InSecondsF();
}

@end
