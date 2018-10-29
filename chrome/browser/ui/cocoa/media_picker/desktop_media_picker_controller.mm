// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/media_picker/desktop_media_picker_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#import "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#import "chrome/browser/ui/cocoa/media_picker/desktop_media_picker_item.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/flipped_view.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia_util_mac.h"

using content::DesktopMediaID;

namespace {

const CGFloat kDesktopMediaPickerInitialContentWidth = 620;
const CGFloat kDesktopMediaPickerMinimumContentWidth = 500;
const CGFloat kDesktopMediaPickerMinimumContentHeight = 390;
const CGFloat kDesktopMediaPickerThumbnailWidth = 150;
const CGFloat kDesktopMediaPickerThumbnailHeight = 150;
const CGFloat kDesktopMediaPickerSingleScreenWidth = 300;
const CGFloat kDesktopMediaPickerSingleScreenHeight = 300;
const CGFloat kDesktopMediaPickerMultipleScreenWidth = 220;
const CGFloat kDesktopMediaPickerMultipleScreenHeight = 220;
const CGFloat kDesktopMediaPickerFramePadding = 20;
const CGFloat kDesktopMediaPickerControlSpacing = 10;
const CGFloat kDesktopMediaPickerExcessButtonPadding = 6;
const CGFloat kDesktopMediaPickerRowHeight = 20;
const CGFloat kDesktopMediaPickerRowWidth = 500;
const CGFloat kDesktopMediaPickerIconWidth = 20;
const CGFloat kDesktopMediaPickerPaddedWidth =
    kDesktopMediaPickerInitialContentWidth -
    (kDesktopMediaPickerFramePadding * 2);
const CGFloat kDesktopMediaPickerFontSize = 13;

NSString* const kDesktopMediaPickerIconId = @"icon";
NSString* const kDesktopMediaPickerTitleId = @"title";

}  // namespace

@interface DesktopMediaPickerController ()

// Populate the window with controls and views.
- (void)initializeContentsWithAppName:(const base::string16&)appName
                           targetName:(const base::string16&)targetName
                         requestAudio:(bool)requestAudio;

// Add |NSSegmentControl| for source type switch.
- (void)createTypeButtonAtOrigin:(NSPoint)origin;

// Add |IKImageBrowerView| for screen and window source view.
// Add |NSTableView| for tab source view.
- (void)createSourceViewsAtOrigin:(NSPoint)origin;

// Add check box for audio sharing.
- (void)createAudioCheckboxAtOrigin:(NSPoint)origin;

// Create the share and cancel button.
- (void)createActionButtonsAtOrigin:(NSPoint)origin;

// Create a |NSTextField| with label traits given |width|. Frame height is
// automatically adjusted to fit.
- (NSTextField*)createTextFieldWithText:(NSString*)text
                             frameWidth:(CGFloat)width;

// Create a button with |title|, with size adjusted to fit.
- (NSButton*)createButtonWithTitle:(NSString*)title;

- (IKImageBrowserView*)createImageBrowserWithSize:(NSSize)size;

// Report result by invoking |doneCallback_|. The callback is invoked only on
// the first call to |reportResult:|. Subsequent calls will be no-ops.
- (void)reportResult:(DesktopMediaID)sourceID;

// Action handlers.
- (void)sharePressed:(id)sender;
- (void)cancelPressed:(id)sender;

// Helper functions to get source type, or get data entities based on source
// type.
- (DesktopMediaID::Type)selectedSourceType;
- (DesktopMediaID::Type)sourceTypeForBrowser:(id)browser;
- (id)browserViewForType:(DesktopMediaID::Type)sourceType;
- (NSMutableArray*)itemSetForType:(DesktopMediaID::Type)sourceType;
- (NSInteger)selectedIndexForType:(DesktopMediaID::Type)sourceType;
- (void)setTabBrowserIndex:(NSInteger)index;

@end

@implementation DesktopMediaPickerController

- (id)initWithSourceLists:
          (std::vector<std::unique_ptr<DesktopMediaList>>)sourceLists
                   parent:(NSWindow*)parent
                 callback:(const DesktopMediaPicker::DoneCallback&)callback
                  appName:(const base::string16&)appName
               targetName:(const base::string16&)targetName
             requestAudio:(bool)requestAudio {
  const NSUInteger kStyleMask =
      NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask;
  base::scoped_nsobject<NSWindow> window(
      [[NSWindow alloc] initWithContentRect:ui::kWindowSizeDeterminedLater
                                  styleMask:kStyleMask
                                    backing:NSBackingStoreBuffered
                                      defer:NO]);

  if ((self = [super initWithWindow:window])) {
    [parent addChildWindow:window ordered:NSWindowAbove];
    [window setDelegate:self];

    sourceLists_ = std::move(sourceLists);
    for (auto& sourceList : sourceLists_) {
      switch (sourceList->GetMediaListType()) {
        case DesktopMediaID::TYPE_NONE:
          NOTREACHED();
          break;
        case DesktopMediaID::TYPE_SCREEN:
          screenItems_.reset([[NSMutableArray alloc] init]);
          break;
        case DesktopMediaID::TYPE_WINDOW:
          sourceList->SetViewDialogWindowId(DesktopMediaID(
              DesktopMediaID::TYPE_WINDOW, [window windowNumber]));
          windowItems_.reset([[NSMutableArray alloc] init]);
          break;
        case DesktopMediaID::TYPE_WEB_CONTENTS:
          tabItems_.reset([[NSMutableArray alloc] init]);
          break;
      }
    }

    [self initializeContentsWithAppName:appName
                             targetName:targetName
                           requestAudio:requestAudio];
    doneCallback_ = callback;

    bridge_.reset(new DesktopMediaPickerBridge(self));
  }
  return self;
}

- (void)dealloc {
  [shareButton_ setTarget:nil];
  [cancelButton_ setTarget:nil];
  [screenBrowser_ setDelegate:nil];
  [screenBrowser_ setDataSource:nil];
  [windowBrowser_ setDelegate:nil];
  [windowBrowser_ setDataSource:nil];
  [tabBrowser_ setDataSource:nil];
  [tabBrowser_ setDelegate:nil];
  [[self window] close];
  [super dealloc];
}

- (void)initializeContentsWithAppName:(const base::string16&)appName
                           targetName:(const base::string16&)targetName
                         requestAudio:(bool)requestAudio {
  // Use flipped coordinates to facilitate manual layout.
  base::scoped_nsobject<FlippedView> content(
      [[FlippedView alloc] initWithFrame:NSZeroRect]);
  [[self window] setContentView:content];
  NSPoint origin = NSMakePoint(kDesktopMediaPickerFramePadding,
                               kDesktopMediaPickerFramePadding);

  // Set the dialog's title.
  NSString* titleText = l10n_util::GetNSString(IDS_DESKTOP_MEDIA_PICKER_TITLE);
  [[self window] setTitle:titleText];

  // Set the dialog's description.
  NSString* descriptionText;
  if (appName == targetName) {
    descriptionText = l10n_util::GetNSStringF(
        IDS_DESKTOP_MEDIA_PICKER_TEXT, appName);
  } else {
    descriptionText = l10n_util::GetNSStringF(
        IDS_DESKTOP_MEDIA_PICKER_TEXT_DELEGATED, appName, targetName);
  }
  NSTextField* description =
      [self createTextFieldWithText:descriptionText
                         frameWidth:kDesktopMediaPickerPaddedWidth];
  [description setFrameOrigin:origin];
  [content addSubview:description];
  origin.y += NSHeight([description frame]) + kDesktopMediaPickerControlSpacing;

  [self createTypeButtonAtOrigin:origin];
  origin.y +=
      NSHeight([sourceTypeControl_ frame]) + kDesktopMediaPickerControlSpacing;

  [self createSourceViewsAtOrigin:origin];
  origin.y +=
      NSHeight([imageBrowserScroll_ frame]) + kDesktopMediaPickerControlSpacing;

  if (requestAudio) {
    [self createAudioCheckboxAtOrigin:origin];
    origin.y += NSHeight([audioShareCheckbox_ frame]) +
                kDesktopMediaPickerControlSpacing;
  }

  [self createActionButtonsAtOrigin:origin];
  origin.y += kDesktopMediaPickerFramePadding +
              (NSHeight([cancelButton_ frame]) -
               kDesktopMediaPickerExcessButtonPadding);

  // Resize window to fit.
  [content setAutoresizesSubviews:NO];
  [[self window]
      setContentSize:NSMakeSize(kDesktopMediaPickerInitialContentWidth,
                                origin.y)];
  [[self window]
      setContentMinSize:NSMakeSize(kDesktopMediaPickerMinimumContentWidth,
                                   kDesktopMediaPickerMinimumContentHeight)];
  [content setAutoresizesSubviews:YES];

  // Initialize the type selection at the first segment.
  [sourceTypeControl_ setSelected:YES forSegment:0];
  [self typeButtonPressed:sourceTypeControl_];
  [[self window]
      makeFirstResponder:[self browserViewForType:[self selectedSourceType]]];
}

- (void)createTypeButtonAtOrigin:(NSPoint)origin {
  // Create segmented button.
  sourceTypeControl_.reset(
      [[NSSegmentedControl alloc] initWithFrame:NSZeroRect]);

  NSInteger segmentCount = sourceLists_.size();
  [sourceTypeControl_ setSegmentCount:segmentCount];
  NSInteger segmentIndex = 0;

  for (auto& sourceList : sourceLists_) {
    switch (sourceList->GetMediaListType()) {
      case DesktopMediaID::TYPE_NONE:
        NOTREACHED();
        break;
      case DesktopMediaID::TYPE_SCREEN:
        [sourceTypeControl_
              setLabel:l10n_util::GetNSString(
                           IDS_DESKTOP_MEDIA_PICKER_SOURCE_TYPE_SCREEN)
            forSegment:segmentIndex];

        [[sourceTypeControl_ cell] setTag:DesktopMediaID::TYPE_SCREEN
                               forSegment:segmentIndex];
        break;
      case DesktopMediaID::TYPE_WINDOW:
        [sourceTypeControl_
              setLabel:l10n_util::GetNSString(
                           IDS_DESKTOP_MEDIA_PICKER_SOURCE_TYPE_WINDOW)
            forSegment:segmentIndex];
        [[sourceTypeControl_ cell] setTag:DesktopMediaID::TYPE_WINDOW
                               forSegment:segmentIndex];
        break;
      case DesktopMediaID::TYPE_WEB_CONTENTS:
        [sourceTypeControl_
              setLabel:l10n_util::GetNSString(
                           IDS_DESKTOP_MEDIA_PICKER_SOURCE_TYPE_TAB)
            forSegment:segmentIndex];
        [[sourceTypeControl_ cell] setTag:DesktopMediaID::TYPE_WEB_CONTENTS
                               forSegment:segmentIndex];
        break;
    }
    ++segmentIndex;
  }

  [sourceTypeControl_ setTarget:self];
  [sourceTypeControl_ setAction:@selector(typeButtonPressed:)];

  [[sourceTypeControl_ cell] setTrackingMode:NSSegmentSwitchTrackingSelectOne];

  [[[self window] contentView] addSubview:sourceTypeControl_];

  [sourceTypeControl_ sizeToFit];
  [sourceTypeControl_ setAutoresizingMask:NSViewMaxXMargin | NSViewMinXMargin];
  CGFloat controlWidth = NSWidth([sourceTypeControl_ frame]);
  CGFloat controlHeight = NSHeight([sourceTypeControl_ frame]);
  NSRect centerFrame =
      NSMakeRect((kDesktopMediaPickerInitialContentWidth - controlWidth) / 2,
                 origin.y, controlWidth, controlHeight);

  [sourceTypeControl_ setFrame:NSIntegralRect(centerFrame)];
}

- (void)createSourceViewsAtOrigin:(NSPoint)origin {
  for (auto& sourceList : sourceLists_) {
    switch (sourceList->GetMediaListType()) {
      case DesktopMediaID::TYPE_NONE: {
        NOTREACHED();
        break;
      }
      case DesktopMediaID::TYPE_SCREEN: {
        const bool is_single = sourceList->GetSourceCount() <= 1;
        const CGFloat width = is_single
                                  ? kDesktopMediaPickerSingleScreenWidth
                                  : kDesktopMediaPickerMultipleScreenWidth;
        const CGFloat height = is_single
                                   ? kDesktopMediaPickerSingleScreenHeight
                                   : kDesktopMediaPickerMultipleScreenHeight;
        screenBrowser_.reset([[self
            createImageBrowserWithSize:NSMakeSize(width, height)] retain]);
        break;
      }

      case DesktopMediaID::TYPE_WINDOW: {
        windowBrowser_.reset([[self
            createImageBrowserWithSize:NSMakeSize(
                                           kDesktopMediaPickerThumbnailWidth,
                                           kDesktopMediaPickerThumbnailHeight)]
            retain]);
        break;
      }
      case DesktopMediaID::TYPE_WEB_CONTENTS: {
        tabBrowser_.reset([[NSTableView alloc] initWithFrame:NSZeroRect]);
        [tabBrowser_ setDelegate:self];
        [tabBrowser_ setDataSource:self];
        [tabBrowser_ setAllowsMultipleSelection:NO];
        [tabBrowser_ setRowHeight:kDesktopMediaPickerRowHeight];
        [tabBrowser_ setDoubleAction:@selector(sharePressed:)];
        base::scoped_nsobject<NSTableColumn> iconColumn([[NSTableColumn alloc]
            initWithIdentifier:kDesktopMediaPickerIconId]);
        [iconColumn setEditable:NO];
        [iconColumn setWidth:kDesktopMediaPickerIconWidth];
        [tabBrowser_ addTableColumn:iconColumn];
        base::scoped_nsobject<NSTableColumn> titleColumn([[NSTableColumn alloc]
            initWithIdentifier:kDesktopMediaPickerTitleId]);
        [titleColumn setEditable:NO];
        [titleColumn setWidth:kDesktopMediaPickerRowWidth];
        [tabBrowser_ addTableColumn:titleColumn];
        [tabBrowser_ setHeaderView:nil];
        break;
      }
    }
  }
  // Create a scroll view to host the image browsers.
  NSRect imageBrowserScrollFrame =
      NSMakeRect(origin.x, origin.y, kDesktopMediaPickerPaddedWidth, 350);
  imageBrowserScroll_.reset(
      [[NSScrollView alloc] initWithFrame:imageBrowserScrollFrame]);
  [imageBrowserScroll_ setHasVerticalScroller:YES];
  [imageBrowserScroll_ setBorderType:NSBezelBorder];
  [imageBrowserScroll_
      setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [[[self window] contentView] addSubview:imageBrowserScroll_];
}

- (void)createAudioCheckboxAtOrigin:(NSPoint)origin {
  audioShareCheckbox_.reset([[NSButton alloc] initWithFrame:NSZeroRect]);
  [audioShareCheckbox_ setFrameOrigin:origin];
  [audioShareCheckbox_ setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
  [audioShareCheckbox_ setButtonType:NSSwitchButton];
  [audioShareCheckbox_ setState:NSOnState];
  [audioShareCheckbox_
      setTitle:l10n_util::GetNSString(IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE)];
  [audioShareCheckbox_ sizeToFit];
  [[[self window] contentView] addSubview:audioShareCheckbox_];
}

- (void)createActionButtonsAtOrigin:(NSPoint)origin {
  FlippedView* content = [[self window] contentView];

  // Create the share button.
  shareButton_ =
      [self createButtonWithTitle:l10n_util::GetNSString(
                                      IDS_DESKTOP_MEDIA_PICKER_SHARE)];
  origin.x =
      kDesktopMediaPickerInitialContentWidth - kDesktopMediaPickerFramePadding -
      (NSWidth([shareButton_ frame]) - kDesktopMediaPickerExcessButtonPadding);
  [shareButton_ setEnabled:NO];
  [shareButton_ setFrameOrigin:origin];
  [shareButton_ setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [shareButton_ setTarget:self];
  [shareButton_ setKeyEquivalent:kKeyEquivalentReturn];
  [shareButton_ setAction:@selector(sharePressed:)];
  [content addSubview:shareButton_];

  // Create the cancel button.
  cancelButton_ =
      [self createButtonWithTitle:l10n_util::GetNSString(IDS_CANCEL)];
  origin.x -= kDesktopMediaPickerControlSpacing +
              (NSWidth([cancelButton_ frame]) -
               (kDesktopMediaPickerExcessButtonPadding * 2));
  [cancelButton_ setFrameOrigin:origin];
  [cancelButton_ setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [cancelButton_ setTarget:self];
  [cancelButton_ setKeyEquivalent:kKeyEquivalentEscape];
  [cancelButton_ setAction:@selector(cancelPressed:)];
  [content addSubview:cancelButton_];
}

- (NSTextField*)createTextFieldWithText:(NSString*)text
                             frameWidth:(CGFloat)width {
  NSRect frame = NSMakeRect(0, 0, width, 1);
  base::scoped_nsobject<NSTextField> textField(
      [[NSTextField alloc] initWithFrame:frame]);
  [textField setEditable:NO];
  [textField setSelectable:YES];
  [textField setDrawsBackground:NO];
  [textField setBezeled:NO];
  [textField setStringValue:text];
  [textField setFont:[NSFont systemFontOfSize:kDesktopMediaPickerFontSize]];
  [textField setAutoresizingMask:NSViewWidthSizable];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:textField];
  return textField.autorelease();
}

- (NSButton*)createButtonWithTitle:(NSString*)title {
  base::scoped_nsobject<NSButton> button(
      [[NSButton alloc] initWithFrame:NSZeroRect]);
  [button setButtonType:NSMomentaryPushInButton];
  [button setBezelStyle:NSRoundedBezelStyle];
  [button setTitle:title];
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:button];
  return button.autorelease();
}

- (IKImageBrowserView*)createImageBrowserWithSize:(NSSize)size {
  NSUInteger cellStyle = IKCellsStyleShadowed | IKCellsStyleTitled;
  base::scoped_nsobject<IKImageBrowserView> browser(
      [[IKImageBrowserView alloc] initWithFrame:NSZeroRect]);
  [browser setDelegate:self];
  [browser setDataSource:self];
  [browser setCellsStyleMask:cellStyle];
  [browser setCellSize:size];
  [browser setAllowsMultipleSelection:NO];
  return browser.autorelease();
}

#pragma mark Event Actions

- (void)showWindow:(id)sender {
  // Signal the source lists to start sending thumbnails. |bridge_| is used as
  // the observer, and will forward notifications to this object.
  for (auto& sourceList : sourceLists_) {
    switch (sourceList->GetMediaListType()) {
      case DesktopMediaID::TYPE_NONE: {
        NOTREACHED();
        break;
      }
      case DesktopMediaID::TYPE_SCREEN: {
        sourceList->SetThumbnailSize(
            gfx::Size(kDesktopMediaPickerSingleScreenWidth,
                      kDesktopMediaPickerSingleScreenHeight));
        sourceList->StartUpdating(bridge_.get());
        break;
      }
      case DesktopMediaID::TYPE_WINDOW: {
        sourceList->SetThumbnailSize(
            gfx::Size(kDesktopMediaPickerThumbnailWidth,
                      kDesktopMediaPickerThumbnailHeight));
        sourceList->StartUpdating(bridge_.get());
        break;
      }
      case DesktopMediaID::TYPE_WEB_CONTENTS: {
        sourceList->SetThumbnailSize(gfx::Size(kDesktopMediaPickerIconWidth,
                                               kDesktopMediaPickerRowHeight));
        sourceList->StartUpdating(bridge_.get());
        break;
      }
    }
  }

  [self.window center];
  [super showWindow:sender];
}

- (void)reportResult:(DesktopMediaID)sourceID {
  if (doneCallback_.is_null()) {
    return;
  }

  sourceID.audio_share = ![audioShareCheckbox_ isHidden] &&
                         [audioShareCheckbox_ state] == NSOnState;

  // If the media source is an tab, activate it.
  if (sourceID.type == DesktopMediaID::TYPE_WEB_CONTENTS) {
    content::WebContents* tab = content::WebContents::FromRenderFrameHost(
        content::RenderFrameHost::FromID(
            sourceID.web_contents_id.render_process_id,
            sourceID.web_contents_id.main_render_frame_id));
    if (tab) {
      tab->GetDelegate()->ActivateContents(tab);
      Browser* browser = chrome::FindBrowserWithWebContents(tab);
      if (browser && browser->window())
        browser->window()->Activate();
    }
  }

  // Notify the |callback_| asynchronously because it may release the
  // controller.
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::Bind(doneCallback_, sourceID));
  doneCallback_.Reset();
}

- (void)sharePressed:(id)sender {
  DesktopMediaID::Type selectedType = [self selectedSourceType];
  NSMutableArray* items = [self itemSetForType:selectedType];
  NSInteger selectedIndex = [self selectedIndexForType:selectedType];
  DesktopMediaPickerItem* item = [items objectAtIndex:selectedIndex];
  [self reportResult:[item sourceID]];
  [self close];
}

- (void)cancelPressed:(id)sender {
  [self reportResult:DesktopMediaID()];
  [self close];
}

- (void)typeButtonPressed:(id)sender {
  DesktopMediaID::Type selectedType = [self selectedSourceType];
  id browser = [self browserViewForType:selectedType];

  [audioShareCheckbox_
      setHidden:selectedType != DesktopMediaID::TYPE_WEB_CONTENTS];
  [imageBrowserScroll_ setDocumentView:browser];

  if (selectedType == DesktopMediaID::TYPE_WEB_CONTENTS) {
    NSInteger selectedIndex = [self selectedIndexForType:selectedType];
    [tabBrowser_ reloadData];
    [self setTabBrowserIndex:selectedIndex];
  } else {
    [browser reloadData];
    [self imageBrowserSelectionDidChange:browser];
  }
}

#pragma mark Data Retrieve Helper

- (DesktopMediaID::Type)selectedSourceType {
  NSInteger segment = [sourceTypeControl_ selectedSegment];
  return static_cast<DesktopMediaID::Type>(
      [[sourceTypeControl_ cell] tagForSegment:segment]);
}

- (DesktopMediaID::Type)sourceTypeForBrowser:(id)browser {
  if (browser == screenBrowser_.get())
    return DesktopMediaID::TYPE_SCREEN;
  if (browser == windowBrowser_.get())
    return DesktopMediaID::TYPE_WINDOW;
  return DesktopMediaID::TYPE_WEB_CONTENTS;
}

- (id)browserViewForType:(DesktopMediaID::Type)sourceType {
  switch (sourceType) {
    case DesktopMediaID::TYPE_SCREEN:
      return screenBrowser_;
    case DesktopMediaID::TYPE_WINDOW:
      return windowBrowser_;
    case DesktopMediaID::TYPE_WEB_CONTENTS:
      return tabBrowser_;
    case DesktopMediaID::TYPE_NONE:
      NOTREACHED();
      return nil;
  }
}

- (NSMutableArray*)itemSetForType:(DesktopMediaID::Type)sourceType {
  switch (sourceType) {
    case DesktopMediaID::TYPE_SCREEN:
      return screenItems_;
    case DesktopMediaID::TYPE_WINDOW:
      return windowItems_;
    case DesktopMediaID::TYPE_WEB_CONTENTS:
      return tabItems_;
    case DesktopMediaID::TYPE_NONE:
      NOTREACHED();
      return nil;
  }
}

- (NSInteger)selectedIndexForType:(DesktopMediaID::Type)sourceType {
  NSIndexSet* indexes = nil;
  switch (sourceType) {
    case DesktopMediaID::TYPE_SCREEN:
      indexes = [screenBrowser_ selectionIndexes];
      break;
    case DesktopMediaID::TYPE_WINDOW:
      indexes = [windowBrowser_ selectionIndexes];
      break;
    case DesktopMediaID::TYPE_WEB_CONTENTS:
      indexes = [tabBrowser_ selectedRowIndexes];
      break;
    case DesktopMediaID::TYPE_NONE:
      NOTREACHED();
  }

  if ([indexes count] == 0)
    return -1;
  return [indexes firstIndex];
}

- (void)setTabBrowserIndex:(NSInteger)index {
  NSIndexSet* indexes;

  if (index < 0)
    indexes = [NSIndexSet indexSet];
  else
    indexes = [NSIndexSet indexSetWithIndex:index];

  [tabBrowser_ selectRowIndexes:indexes byExtendingSelection:NO];
}

#pragma mark NSWindowDelegate

- (void)windowWillClose:(NSNotification*)notification {
  // Report the result if it hasn't been reported yet. |reportResult:| ensures
  // that the result is only reported once.
  [self reportResult:DesktopMediaID()];

  // Remove self from the parent.
  NSWindow* window = [self window];
  [[window parentWindow] removeChildWindow:window];
}

#pragma mark IKImageBrowserDataSource

- (NSUInteger)numberOfItemsInImageBrowser:(IKImageBrowserView*)browser {
  DesktopMediaID::Type sourceType = [self sourceTypeForBrowser:browser];
  NSMutableArray* items = [self itemSetForType:sourceType];
  return [items count];
}

- (id)imageBrowser:(IKImageBrowserView*)browser itemAtIndex:(NSUInteger)index {
  DesktopMediaID::Type sourceType = [self sourceTypeForBrowser:browser];
  NSMutableArray* items = [self itemSetForType:sourceType];
  DesktopMediaPickerItem* item = [items objectAtIndex:index];

  // For screen source, if there is only one source, we can omit the label
  // "Entire Screen", because it is redundant with tab label "Your Entire
  // Screen".
  [item setTitleHidden:browser == screenBrowser_ && [items count] == 1];
  return item;
}

#pragma mark IKImageBrowserDelegate

- (void)imageBrowser:(IKImageBrowserView*)browser
    cellWasDoubleClickedAtIndex:(NSUInteger)index {
  DesktopMediaPickerItem* item;
  if (browser == screenBrowser_)
    item = [screenItems_ objectAtIndex:index];
  else
    item = [windowItems_ objectAtIndex:index];
  [self reportResult:[item sourceID]];
  [self close];
}

- (void)imageBrowserSelectionDidChange:(IKImageBrowserView*)browser {
  DesktopMediaID::Type selectedType = [self selectedSourceType];
  NSInteger selectedIndex = [self selectedIndexForType:selectedType];
  // Enable or disable the OK button based on whether we have a selection.
  [shareButton_ setEnabled:(selectedIndex >= 0)];
}

#pragma mark NSTableViewDataSource

- (NSInteger)numberOfRowsInTableView:(NSTableView*)table {
  return [tabItems_ count];
}

#pragma mark NSTableViewDelegate

- (NSView*)tableView:(NSTableView*)table
    viewForTableColumn:(NSTableColumn*)column
                   row:(NSInteger)rowIndex {
  if ([[column identifier] isEqualToString:kDesktopMediaPickerIconId]) {
    NSImage* image = [[tabItems_ objectAtIndex:rowIndex] imageRepresentation];
    base::scoped_nsobject<NSImageView> iconView(
        [[table makeViewWithIdentifier:kDesktopMediaPickerIconId owner:self]
            retain]);
    if (!iconView) {
      iconView.reset([[NSImageView alloc]
          initWithFrame:NSMakeRect(0, 0, kDesktopMediaPickerIconWidth,
                                   kDesktopMediaPickerRowWidth)]);
      [iconView setIdentifier:kDesktopMediaPickerIconId];
    }
    [iconView setImage:image];
    return iconView.autorelease();
  }

  NSString* string = [[tabItems_ objectAtIndex:rowIndex] imageTitle];
  base::scoped_nsobject<NSTextField> titleView(
      [[table makeViewWithIdentifier:kDesktopMediaPickerTitleId owner:self]
          retain]);
  if (!titleView) {
    titleView.reset(
        [[self createTextFieldWithText:string
                            frameWidth:kDesktopMediaPickerMinimumContentWidth]
            retain]);
    [titleView setIdentifier:kDesktopMediaPickerTitleId];
  } else {
    [titleView setStringValue:string];
  }
  return titleView.autorelease();
}

- (void)tableViewSelectionDidChange:(NSNotification*)notification {
  NSIndexSet* indexes = [tabBrowser_ selectedRowIndexes];

  // Enable or disable the OK button based on whether we have a selection.
  [shareButton_ setEnabled:([indexes count] > 0)];
}

#pragma mark DesktopMediaPickerObserver

- (void)sourceAddedForList:(DesktopMediaList*)list atIndex:(int)index {
  DesktopMediaID::Type sourceType = list->GetMediaListType();
  NSMutableArray* items = [self itemSetForType:sourceType];
  id browser = [self browserViewForType:sourceType];
  NSInteger selectedIndex = [self selectedIndexForType:sourceType];
  if (selectedIndex >= index)
    ++selectedIndex;

  const DesktopMediaList::Source& source = list->GetSource(index);
  NSString* imageTitle = base::SysUTF16ToNSString(source.name);
  base::scoped_nsobject<DesktopMediaPickerItem> item(
      [[DesktopMediaPickerItem alloc] initWithSourceId:source.id
                                              imageUID:++lastImageUID_
                                            imageTitle:imageTitle]);

  [items insertObject:item atIndex:index];
  [browser reloadData];
  if (sourceType == DesktopMediaID::TYPE_WEB_CONTENTS) {
    // Memorizing selection.
    [self setTabBrowserIndex:selectedIndex];
  } else if (sourceType == DesktopMediaID::TYPE_SCREEN) {
    if ([items count] == 1) {
      // Preselect the first screen source.
      [browser setSelectionIndexes:[NSIndexSet indexSetWithIndex:0]
              byExtendingSelection:NO];
    } else if ([items count] == 2) {
      // Switch to multiple sources mode.
      [browser setCellSize:NSMakeSize(kDesktopMediaPickerMultipleScreenWidth,
                                      kDesktopMediaPickerMultipleScreenHeight)];
    }
  }

  NSString* autoselectSource = base::SysUTF8ToNSString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAutoSelectDesktopCaptureSource));

  if ([autoselectSource isEqualToString:imageTitle]) {
    [self reportResult:[item sourceID]];
    [self close];
  }
}

- (void)sourceRemovedForList:(DesktopMediaList*)list atIndex:(int)index {
  DesktopMediaID::Type sourceType = list->GetMediaListType();
  NSMutableArray* items = [self itemSetForType:sourceType];
  id browser = [self browserViewForType:sourceType];

  if (sourceType == DesktopMediaID::TYPE_WEB_CONTENTS) {
    NSInteger selectedIndex = [self selectedIndexForType:sourceType];
    if (selectedIndex > index)
      --selectedIndex;
    else if (selectedIndex == index)
      selectedIndex = -1;
    [tabItems_ removeObjectAtIndex:index];
    [tabBrowser_ reloadData];
    [self setTabBrowserIndex:selectedIndex];
    return;
  }

  if ([[browser selectionIndexes] containsIndex:index]) {
    // Selected item was removed. Clear selection.
    [browser setSelectionIndexes:[NSIndexSet indexSet] byExtendingSelection:NO];
  }
  [items removeObjectAtIndex:index];
  if (sourceType == DesktopMediaID::TYPE_SCREEN && [items count] == 1)
    [browser setCellSize:NSMakeSize(kDesktopMediaPickerSingleScreenWidth,
                                    kDesktopMediaPickerSingleScreenHeight)];
  [browser reloadData];
}

- (void)sourceMovedForList:(DesktopMediaList*)list
                      from:(int)oldIndex
                        to:(int)newIndex {
  DesktopMediaID::Type sourceType = list->GetMediaListType();
  NSMutableArray* items = [self itemSetForType:sourceType];
  id browser = [self browserViewForType:sourceType];
  NSInteger selectedIndex = [self selectedIndexForType:sourceType];
  if (selectedIndex > oldIndex && selectedIndex <= newIndex)
    --selectedIndex;
  else if (selectedIndex < oldIndex && selectedIndex >= newIndex)
    ++selectedIndex;
  else if (selectedIndex == oldIndex)
    selectedIndex = newIndex;

  base::scoped_nsobject<DesktopMediaPickerItem> item(
      [[items objectAtIndex:oldIndex] retain]);
  [items removeObjectAtIndex:oldIndex];
  [items insertObject:item atIndex:newIndex];
  [browser reloadData];

  if (sourceType == DesktopMediaID::TYPE_WEB_CONTENTS)
    [self setTabBrowserIndex:selectedIndex];
}

- (void)sourceNameChangedForList:(DesktopMediaList*)list atIndex:(int)index {
  DesktopMediaID::Type sourceType = list->GetMediaListType();
  NSMutableArray* items = [self itemSetForType:sourceType];
  id browser = [self browserViewForType:sourceType];
  NSInteger selectedIndex = [self selectedIndexForType:sourceType];

  DesktopMediaPickerItem* item = [items objectAtIndex:index];
  const DesktopMediaList::Source& source = list->GetSource(index);
  [item setImageTitle:base::SysUTF16ToNSString(source.name)];
  [browser reloadData];
  if (sourceType == DesktopMediaID::TYPE_WEB_CONTENTS)
    [self setTabBrowserIndex:selectedIndex];
}

- (void)sourceThumbnailChangedForList:(DesktopMediaList*)list
                              atIndex:(int)index {
  DesktopMediaID::Type sourceType = list->GetMediaListType();
  NSMutableArray* items = [self itemSetForType:sourceType];
  id browser = [self browserViewForType:sourceType];
  NSInteger selectedIndex = [self selectedIndexForType:sourceType];

  const DesktopMediaList::Source& source = list->GetSource(index);
  NSImage* image = gfx::NSImageFromImageSkia(source.thumbnail);

  DesktopMediaPickerItem* item = [items objectAtIndex:index];
  [item setImageRepresentation:image];
  [browser reloadData];

  if (sourceType == DesktopMediaID::TYPE_WEB_CONTENTS)
    [self setTabBrowserIndex:selectedIndex];
}

@end  // @interface DesktopMediaPickerController
