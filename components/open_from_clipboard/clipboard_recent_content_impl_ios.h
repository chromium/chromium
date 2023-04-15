// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_IMPL_IOS_H_
#define COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_IMPL_IOS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

typedef NSString* ContentType NS_TYPED_ENUM;

extern ContentType const ContentTypeURL;
extern ContentType const ContentTypeText;
extern ContentType const ContentTypeImage;

// A protocol implemented by delegates to handle clipboard changes.
@protocol ClipboardRecentContentDelegate<NSObject>

- (void)onClipboardChanged;

@end

// Helper class returning a URL if the content of the clipboard can be turned
// into a URL, and if it estimates that the content of the clipboard is not too
// old.
@interface ClipboardRecentContentImplIOS : NSObject

// |delegate| is used for metrics logging and can be nil. |authorizedSchemes|
// should contain all schemes considered valid. |groupUserDefaults| is the
// NSUserDefaults used to store information on pasteboard entry expiration. This
// information will be shared with other applications in the application group.
// |onlyUseClipboardAsync| holds whether the clipboard should only be access
// asynchronously.
- (instancetype)initWithMaxAge:(NSTimeInterval)maxAge
             authorizedSchemes:(NSSet<NSString*>*)authorizedSchemes
                  userDefaults:(NSUserDefaults*)groupUserDefaults
         onlyUseClipboardAsync:(BOOL)onlyUseClipboardAsync
                      delegate:(id<ClipboardRecentContentDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Returns the copied URL if the clipboard contains a recent URL that has not
// been suppressed and will not trigger a pasteboard access notification.
// Otherwise, returns nil.
- (NSURL*)recentURLFromClipboard;

// Returns the copied string if the clipboard contains a recent string that has
// not been suppresed and will not trigger a pasteboard access notification.
// Otherwise, returns nil.
- (NSString*)recentTextFromClipboard;

// Returns the copied image if the clipboard contains a recent image that has
// not been suppressed and will not trigger a pasteboard access notification.
// Otherwise, returns nil.
- (UIImage*)recentImageFromClipboard;

// Returns the set of content types being currently used on the clipboard; will
// be nil if the current pasteboard contents are unknown, or if the clipboard
// content age is expired.
- (NSSet<ContentType>*)cachedClipboardContentTypes;

// Uses the new iOS 14 pasteboard detection pattern API to asynchronously detect
// if the clipboard contains content (that has not been suppressed) of the
// requested types without actually getting the contents.
- (void)hasContentMatchingTypes:(NSSet<ContentType>*)types
              completionHandler:
                  (void (^)(NSSet<ContentType>*))completionHandler;
// Uses the new iOS 14 pasteboard detection pattern API to asynchronously get a
// copied URL from the clipboard if it has not been suppressed. Passes nil to
// the callback otherwise.
- (void)recentURLFromClipboardAsync:(void (^)(NSURL*))callback;
// Uses the new iOS 14 pasteboard detection pattern API to asynchronously get a
// copied string from the clipboard if it has not been suppressed. Passes nil to
// the callback otherwise.
- (void)recentTextFromClipboardAsync:(void (^)(NSString*))callback;
// Asynchronously gets an image from the clipboard if is has not been
// suppressed. Passes nil to the callback otherwise. This does not actually use
// any iOS 14 APIs and could be done synchronously, but is here for consistency.
- (void)recentImageFromClipboardAsync:(void (^)(UIImage*))callback;

// Returns how old the content of the clipboard is.
- (NSTimeInterval)clipboardContentAge;

// Prevents GetRecentURLFromClipboard from returning anything until the
// clipboard's content changes.
- (void)suppressClipboardContent;

// Methods below are exposed for testing purposes.

// Estimation of the date when the pasteboard changed.
@property(nonatomic, copy) NSDate* lastPasteboardChangeDate;

// Saves information to the user defaults about the latest pasteboard entry.
- (void)saveToUserDefaults;

@end

#endif  // COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_IMPL_IOS_H_
