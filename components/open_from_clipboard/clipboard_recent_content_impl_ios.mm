// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/open_from_clipboard/clipboard_recent_content_impl_ios.h"

#import <MobileCoreServices/MobileCoreServices.h>
#import <UIKit/UIKit.h>

#import "base/mac/foundation_util.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

// If the content of the pasteboard has changed, updates the change count
// and change date.
- (void)updateIfNeeded;

// Returns whether the pasteboard changed since the last time a pasteboard
// change was detected.
- (BOOL)hasPasteboardChanged;

// Loads information from the user defaults about the latest pasteboard entry.
- (void)loadFromUserDefaults;

// Returns the URL contained in the clipboard (if any).
- (NSURL*)URLFromPasteboard;

// Returns the uptime.
- (NSTimeInterval)uptime;

// Returns whether the value of the clipboard should be returned.
- (BOOL)shouldReturnValueOfClipboard;

// Calls |completionHandler| with the result of whether or not the clipboard
// currently contains data matching |contentType|.
- (void)checkForContentType:(ContentType)contentType
          completionHandler:(void (^)(BOOL))completionHandler;

// Checks the clipboard for content matching |types| and calls
// |completionHandler| once all types are checked. This method is called
// recursively and partial results are passed in |results| until all types have
// been checked.
- (void)
    hasContentMatchingRemainingTypes:(NSSet<ContentType>*)types
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
                      delegate:(id<ClipboardRecentContentDelegate>)delegate {
  self = [super init];
  if (self) {
    _maximumAgeOfClipboard = maxAge;
    _delegate = delegate;
    _authorizedSchemes = authorizedSchemes;
    _sharedUserDefaults = groupUserDefaults;

    _lastPasteboardChangeCount = NSIntegerMax;
    [self loadFromUserDefaults];
    [self updateIfNeeded];
    [self updateCachedClipboardState];

    // Makes sure |last_pasteboard_change_count_| was properly initialized.
    DCHECK_NE(_lastPasteboardChangeCount, NSIntegerMax);
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
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)didBecomeActive:(NSNotification*)notification {
  [self loadFromUserDefaults];
  [self updateIfNeeded];
  [self updateCachedClipboardState];
}

- (BOOL)hasPasteboardChanged {
  return UIPasteboard.generalPasteboard.changeCount !=
         self.lastPasteboardChangeCount;
}

- (void)pasteboardDidChange:(NSNotification*)notification {
  [self updateCachedClipboardState];
}

- (NSURL*)recentURLFromClipboard {
  [self updateIfNeeded];

  if (![self shouldReturnValueOfClipboard])
    return nil;

  if (@available(iOS 14, *)) {
    // On iOS 14, don't actually access the pasteboard in this method. This
    // prevents the pasteboard access notification from appearing.
  } else {
    if (!self.cachedURL) {
      self.cachedURL = [self URLFromPasteboard];
    }
  }
  return self.cachedURL;
}

- (NSString*)recentTextFromClipboard {
  [self updateIfNeeded];

  if (![self shouldReturnValueOfClipboard])
    return nil;

  if (@available(iOS 14, *)) {
    // On iOS 14, don't actually access the pasteboard in this method. This
    // prevents the pasteboard access notification from appearing.
  } else {
    if (!self.cachedText) {
      self.cachedText = UIPasteboard.generalPasteboard.string;
    }
  }
  return self.cachedText;
}

- (UIImage*)recentImageFromClipboard {
  [self updateIfNeeded];

  if (![self shouldReturnValueOfClipboard])
    return nil;

  if (@available(iOS 14, *)) {
    // On iOS 14, don't actually access the pasteboard in this method. This
    // prevents the pasteboard access notification from appearing.
  } else {
    if (!self.cachedImage) {
      self.cachedImage = UIPasteboard.generalPasteboard.image;
    }
  }

  return self.cachedImage;
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

- (NSSet<ContentType>*)cachedClipboardContentTypes {
  if (![self shouldReturnValueOfClipboard])
    return nil;
  return self.cachedContentTypes;
}

- (void)hasContentMatchingTypes:(NSSet<ContentType>*)types
              completionHandler:
                  (void (^)(NSSet<ContentType>*))completionHandler {
  [self updateIfNeeded];
  if (![self shouldReturnValueOfClipboard] || ![types count]) {
    completionHandler([NSSet set]);
    return;
  }

  [self hasContentMatchingRemainingTypes:types
                                 results:[[NSMutableDictionary alloc] init]
                       completionHandler:completionHandler];
}

- (void)
    hasContentMatchingRemainingTypes:(NSSet<ContentType>*)types
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
          completionHandler:^(BOOL hasType) {
            results[type] = @(hasType);

            NSMutableSet* remainingTypes = [types mutableCopy];
            [remainingTypes removeObject:type];
            [weakSelf hasContentMatchingRemainingTypes:remainingTypes
                                               results:results
                                     completionHandler:completionHandler];
          }];
}

- (void)checkForContentType:(ContentType)contentType
          completionHandler:(void (^)(BOOL))completionHandler {
  if ([contentType isEqualToString:ContentTypeText]) {
    [self hasRecentTextFromClipboardInternal:^(BOOL hasText) {
      completionHandler(hasText);
    }];
  } else if ([contentType isEqualToString:ContentTypeURL]) {
    [self hasRecentURLFromClipboardInternal:^(BOOL hasURL) {
      completionHandler(hasURL);
    }];
  } else if ([contentType isEqualToString:ContentTypeImage]) {
    [self hasRecentImageFromClipboardInternal:^(BOOL hasImage) {
      completionHandler(hasImage);
    }];
  } else {
    NOTREACHED() << contentType;
  }
}

- (void)hasRecentURLFromClipboardInternal:(void (^)(BOOL))callback {
  DCHECK(callback);
  if (@available(iOS 14, *)) {
    // Use cached value if it exists
    if (self.cachedURL) {
      callback(YES);
      return;
    }

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
    NSSet<UIPasteboardDetectionPattern>* urlPattern =
        [NSSet setWithObject:UIPasteboardDetectionPatternProbableWebURL];
    [UIPasteboard.generalPasteboard
        detectPatternsForPatterns:urlPattern
                completionHandler:^(
                    NSSet<UIPasteboardDetectionPattern>* patterns,
                    NSError* error) {
                  callback([patterns
                      containsObject:
                          UIPasteboardDetectionPatternProbableWebURL]);
                }];
#else
    // To prevent clipboard notification from appearing on iOS 14 with iOS 13
    // SDK, use the -hasURLs property to check for URL existence. This will
    // cause crbug.com/1033935 to reappear in code using this method (also see
    // the comments in -URLFromPasteboard in this file), but that is preferable
    // to the notificatio appearing when it shouldn't.
    callback(UIPasteboard.generalPasteboard.hasURLs);
#endif
  } else {
    callback([self recentURLFromClipboard] != nil);
  }
}

- (void)hasRecentTextFromClipboardInternal:(void (^)(BOOL))callback {
  DCHECK(callback);
  if (@available(iOS 14, *)) {
    // Use cached value if it exists
    if (self.cachedText) {
      callback(YES);
      return;
    }

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
    NSSet<UIPasteboardDetectionPattern>* textPattern =
        [NSSet setWithObject:UIPasteboardDetectionPatternProbableWebSearch];
    [UIPasteboard.generalPasteboard
        detectPatternsForPatterns:textPattern
                completionHandler:^(
                    NSSet<UIPasteboardDetectionPattern>* patterns,
                    NSError* error) {
                  callback([patterns
                      containsObject:
                          UIPasteboardDetectionPatternProbableWebSearch]);
                }];
#else
    callback(UIPasteboard.generalPasteboard.hasStrings);
#endif
  } else {
    callback([self recentTextFromClipboard] != nil);
  }
}

- (void)hasRecentImageFromClipboardInternal:(void (^)(BOOL))callback {
  DCHECK(callback);
  if (@available(iOS 14, *)) {
    // Use cached value if it exists
    if (self.cachedImage) {
      callback(YES);
      return;
    }

    callback(UIPasteboard.generalPasteboard.hasImages);
  } else {
    callback([self recentImageFromClipboard] != nil);
  }
}

- (void)recentURLFromClipboardAsync:(void (^)(NSURL*))callback {
  DCHECK(callback);
  if (@available(iOS 14, *)) {
    [self updateIfNeeded];
    if (![self shouldReturnValueOfClipboard]) {
      callback(nil);
      return;
    }

    // Use cached value if it exists.
    if (self.cachedURL) {
      callback(self.cachedURL);
      return;
    }

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
    __weak __typeof(self) weakSelf = self;
    NSSet<UIPasteboardDetectionPattern>* urlPattern =
        [NSSet setWithObject:UIPasteboardDetectionPatternProbableWebURL];
    [UIPasteboard.generalPasteboard
        detectValuesForPatterns:urlPattern
              completionHandler:^(
                  NSDictionary<UIPasteboardDetectionPattern, id>* values,
                  NSError* error) {
                // On iOS 16, users can deny access to the clipboard.
                if (error) {
                  weakSelf.cachedURL = nil;
                  callback(nil);
                  return;
                }
                NSURL* url = [NSURL
                    URLWithString:
                        values[UIPasteboardDetectionPatternProbableWebURL]];

                // |detectValuesForPatterns:| will return a url even if the url
                // is missing a scheme. In this case, default to https.
                if (url && url.scheme == nil) {
                  NSURLComponents* components =
                      [[NSURLComponents alloc] initWithURL:url
                                   resolvingAgainstBaseURL:NO];
                  components.scheme = kDefaultScheme;
                  url = components.URL;
                }

                if (![self.authorizedSchemes containsObject:url.scheme]) {
                  weakSelf.cachedURL = nil;
                  callback(nil);
                } else {
                  weakSelf.cachedURL = url;
                  callback(url);
                }
              }];
#else
    callback([self recentURLFromClipboard]);
#endif
  } else {
    callback([self recentURLFromClipboard]);
  }
}

- (void)recentTextFromClipboardAsync:(void (^)(NSString*))callback {
  DCHECK(callback);
  if (@available(iOS 14, *)) {
    [self updateIfNeeded];
    if (![self shouldReturnValueOfClipboard]) {
      callback(nil);
      return;
    }

    // Use cached value if it exists.
    if (self.cachedText) {
      callback(self.cachedText);
      return;
    }

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
    __weak __typeof(self) weakSelf = self;
    NSSet<UIPasteboardDetectionPattern>* textPattern =
        [NSSet setWithObject:UIPasteboardDetectionPatternProbableWebSearch];
    [UIPasteboard.generalPasteboard
        detectValuesForPatterns:textPattern
              completionHandler:^(
                  NSDictionary<UIPasteboardDetectionPattern, id>* values,
                  NSError* error) {
                NSString* text =
                    values[UIPasteboardDetectionPatternProbableWebSearch];
                weakSelf.cachedText = text;

                callback(text);
              }];
#else
    callback([self recentTextFromClipboard]);
#endif
  } else {
    callback([self recentTextFromClipboard]);
  }
}

- (void)recentImageFromClipboardAsync:(void (^)(UIImage*))callback {
  DCHECK(callback);
  [self updateIfNeeded];
  if (![self shouldReturnValueOfClipboard]) {
    callback(nil);
    return;
  }

  if (!self.cachedImage) {
    self.cachedImage = UIPasteboard.generalPasteboard.image;
  }
  callback(self.cachedImage);
}

- (NSTimeInterval)clipboardContentAge {
  return -[self.lastPasteboardChangeDate timeIntervalSinceNow];
}

- (BOOL)shouldReturnValueOfClipboard {
  if ([self clipboardContentAge] > self.maximumAgeOfClipboard)
    return NO;

  // It is the common convention on iOS that password managers tag confidential
  // data with the flavor "org.nspasteboard.ConcealedType". Obey this
  // convention; the user doesn't want for their confidential data to be
  // suggested as a search, anyway. See http://nspasteboard.org/ for more info.
  NSArray<NSString*>* types =
      [[UIPasteboard generalPasteboard] pasteboardTypes];
  if ([types containsObject:@"org.nspasteboard.ConcealedType"])
    return NO;

  return YES;
}

- (void)suppressClipboardContent {
  // User cleared the user data. The pasteboard entry must be removed from the
  // omnibox list. Force entry expiration by setting copy date to 1970.
  self.lastPasteboardChangeDate =
      [[NSDate alloc] initWithTimeIntervalSince1970:0];
  [self saveToUserDefaults];
}

- (void)updateIfNeeded {
  if (![self hasPasteboardChanged]) {
    return;
  }

  self.lastPasteboardChangeDate = [NSDate date];
  self.lastPasteboardChangeCount = [UIPasteboard generalPasteboard].changeCount;

  // Clear the cache because the pasteboard data has changed.
  self.cachedURL = nil;
  self.cachedText = nil;
  self.cachedImage = nil;

  [self.delegate onClipboardChanged];

  [self saveToUserDefaults];
}

- (NSURL*)URLFromPasteboard {
  NSURL* url = [UIPasteboard generalPasteboard].URL;
  // Usually, even if the user copies plaintext, if it looks like a URL, the URL
  // property is filled. Sometimes, this doesn't happen, for instance when the
  // pasteboard is sync'd from a Mac to the iOS simulator. In this case,
  // fallback and manually check whether the pasteboard contains a url-like
  // string.
  if (!url) {
    url = [NSURL URLWithString:UIPasteboard.generalPasteboard.string];
  }
  if (![self.authorizedSchemes containsObject:url.scheme]) {
    return nil;
  }
  return url;
}

- (void)loadFromUserDefaults {
  self.lastPasteboardChangeCount =
      [self.sharedUserDefaults integerForKey:kPasteboardChangeCountKey];
  self.lastPasteboardChangeDate = base::mac::ObjCCastStrict<NSDate>(
      [self.sharedUserDefaults objectForKey:kPasteboardChangeDateKey]);
}

- (void)saveToUserDefaults {
  [self.sharedUserDefaults setInteger:self.lastPasteboardChangeCount
                               forKey:kPasteboardChangeCountKey];
  [self.sharedUserDefaults setObject:self.lastPasteboardChangeDate
                              forKey:kPasteboardChangeDateKey];
}

- (NSTimeInterval)uptime {
  return base::SysInfo::Uptime().InSecondsF();
}

@end
