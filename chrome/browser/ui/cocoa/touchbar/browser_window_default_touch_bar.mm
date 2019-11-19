// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/touchbar/browser_window_default_touch_bar.h"

#include <memory>

#include "base/bind.h"
#include "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/strings/sys_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/command_observer.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#import "chrome/browser/ui/cocoa/touchbar/browser_window_touch_bar_controller.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_member.h"
#include "components/search_engines/util.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#import "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/touch_bar_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

// Touch bar identifiers.
NSString* const kBrowserWindowTouchBarId = @"browser-window";
NSString* const kTabFullscreenTouchBarId = @"tab-fullscreen";

// Touch bar items identifiers.
NSString* const kBackTouchId = @"BACK";
NSString* const kForwardTouchId = @"FORWARD";
NSString* const kReloadOrStopTouchId = @"RELOAD-STOP";
NSString* const kHomeTouchId = @"HOME";
NSString* const kSearchTouchId = @"SEARCH";
NSString* const kStarTouchId = @"BOOKMARK";
NSString* const kNewTabTouchId = @"NEW-TAB";
NSString* const kFullscreenOriginLabelTouchId = @"FULLSCREEN-ORIGIN-LABEL";

// This is a combined back and forward control which can no longer be selected
// but may be in an existing customized Touch Bar. It now represents a group
// containing the back and forward buttons, and adding the back or forward
// buttons to the Touch Bar individually magically decomposes the group.
NSString* const kBackForwardTouchId = @"BACK-FWD";

// Touch bar icon colors values.
const SkColor kTouchBarDefaultIconColor = SK_ColorWHITE;
const SkColor kTouchBarStarActiveColor = gfx::kGoogleBlue500;

const SkColor kTouchBarUrlPathColor = SkColorSetA(SK_ColorWHITE, 0x7F);

// The size of the touch bar icons.
const int kTouchBarIconSize = 16;

// The min width of the search button in the touch bar.
const int kSearchBtnMinWidth = 205;

// Creates an NSImage from the given VectorIcon.
NSImage* CreateNSImageFromIcon(const gfx::VectorIcon& icon,
                               SkColor color = kTouchBarDefaultIconColor) {
  return NSImageFromImageSkiaWithColorSpace(
      gfx::CreateVectorIcon(icon, kTouchBarIconSize, color),
      base::mac::GetSRGBColorSpace());
}

// Creates a NSButton for the touch bar.
API_AVAILABLE(macos(10.12.2))
NSButton* CreateTouchBarButton(const gfx::VectorIcon& icon,
                               BrowserWindowDefaultTouchBar* owner,
                               int command,
                               int tooltip_id,
                               SkColor color = kTouchBarDefaultIconColor) {
  NSButton* button =
      [NSButton buttonWithImage:CreateNSImageFromIcon(icon, color)
                         target:owner
                         action:@selector(executeCommand:)];
  button.tag = command;
  button.accessibilityTitle = l10n_util::GetNSString(tooltip_id);
  return button;
}

ui::TouchBarAction TouchBarActionFromCommand(int command) {
  switch (command) {
    case IDC_BACK:
      return ui::TouchBarAction::BACK;
    case IDC_FORWARD:
      return ui::TouchBarAction::FORWARD;
    case IDC_STOP:
      return ui::TouchBarAction::STOP;
    case IDC_RELOAD:
      return ui::TouchBarAction::RELOAD;
    case IDC_HOME:
      return ui::TouchBarAction::HOME;
    case IDC_FOCUS_LOCATION:
      return ui::TouchBarAction::SEARCH;
    case IDC_BOOKMARK_THIS_TAB:
      return ui::TouchBarAction::STAR;
    case IDC_NEW_TAB:
      return ui::TouchBarAction::NEW_TAB;
    default:
      NOTREACHED();
      return ui::TouchBarAction::TOUCH_BAR_ACTION_COUNT;
  }
}

// A class registered for C++ notifications. This is used to detect changes in
// the profile preferences and the back/forward commands.
class API_AVAILABLE(macos(10.12.2)) TouchBarNotificationBridge
    : public CommandObserver,
      public BrowserListObserver,
      public BookmarkTabHelperObserver,
      public TabStripModelObserver,
      public content::WebContentsObserver {
 public:
  TouchBarNotificationBridge(BrowserWindowDefaultTouchBar* owner,
                             Browser* browser)
      : owner_(owner), browser_(browser), contents_(nullptr) {
    TabStripModel* model = browser_->tab_strip_model();
    DCHECK(model);
    model->AddObserver(this);
    UpdateWebContents(model->GetActiveWebContents());

    auto* command_controller = browser->command_controller();
    command_controller->AddCommandObserver(IDC_BACK, this);
    owner.canGoBack = command_controller->IsCommandEnabled(IDC_BACK);
    command_controller->AddCommandObserver(IDC_FORWARD, this);
    owner.canGoForward = command_controller->IsCommandEnabled(IDC_FORWARD);

    auto* profile = browser->profile();
    auto* prefs = profile->GetPrefs();
    show_home_button_.Init(
        prefs::kShowHomeButton, prefs,
        base::BindRepeating(&TouchBarNotificationBridge::UpdateTouchBar,
                            base::Unretained(this)));

    profile_pref_registrar_.Init(prefs);
    profile_pref_registrar_.Add(
        DefaultSearchManager::kDefaultSearchProviderDataPrefName,
        base::BindRepeating(&TouchBarNotificationBridge::UpdateTouchBar,
                            base::Unretained(this)));

    BrowserList::AddObserver(this);
  }

  bool show_home_button() { return show_home_button_.GetValue(); }

  ~TouchBarNotificationBridge() override {
    BrowserList::RemoveObserver(this);
    browser_->tab_strip_model()->RemoveObserver(this);
    UpdateWebContents(nullptr);
  }

  void UpdateTouchBar() { [[owner_ controller] invalidateTouchBar]; }

  void UpdateWebContents(content::WebContents* new_contents) {
    if (contents_ == new_contents)
      return;
    if (contents_)
      BookmarkTabHelper::FromWebContents(contents_)->RemoveObserver(this);

    contents_ = new_contents;

    // Stop observing the old WebContents and start observing the new one (if
    // nonnull).
    Observe(contents_);

    BookmarkTabHelper* bookmark_helper =
        contents_ ? BookmarkTabHelper::FromWebContents(contents_) : nullptr;
    if (bookmark_helper)
      bookmark_helper->AddObserver(this);

    owner_.isPageLoading = contents_ && contents_->IsLoading();
    owner_.isStarred = bookmark_helper && bookmark_helper->is_starred();
    UpdateTouchBar();
  }

  // BookmarkTabHelperObserver:
  void URLStarredChanged(content::WebContents* web_contents,
                         bool starred) override {
    DCHECK(web_contents == contents_);
    owner_.isStarred = starred;
  }

 protected:
  // CommandObserver:
  void EnabledStateChangedForCommand(int command, bool enabled) override {
    DCHECK(command == IDC_BACK || command == IDC_FORWARD);
    if (command == IDC_BACK)
      owner_.canGoBack = enabled;
    else if (command == IDC_FORWARD)
      owner_.canGoForward = enabled;
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    UpdateWebContents(selection.new_contents);
  }

  void OnBrowserRemoved(Browser* browser) override {
    if (browser == owner_.browser)
      owner_.browser = nullptr;
  }

  // WebContentsObserver:
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override {
    UpdateTouchBar();
  }

  void DidStartLoading() override {
    DCHECK(contents_ && contents_->IsLoading());
    owner_.isPageLoading = YES;
  }

  void DidStopLoading() override {
    DCHECK(contents_ && !contents_->IsLoading());
    owner_.isPageLoading = NO;
  }

  void WebContentsDestroyed() override { UpdateWebContents(nullptr); }

 private:
  BrowserWindowDefaultTouchBar* owner_;  // Weak.
  Browser* browser_;                     // Weak.
  content::WebContents* contents_;       // Weak.

  // Used to monitor the optional home button pref.
  BooleanPrefMember show_home_button_;

  PrefChangeRegistrar profile_pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(TouchBarNotificationBridge);
};

}  // namespace

@interface BrowserWindowDefaultTouchBar () {
  // Used to receive and handle notifications.
  std::unique_ptr<TouchBarNotificationBridge> notificationBridge_;

  // The stop/reload button in the touch bar.
  base::scoped_nsobject<NSButton> reloadStopButton_;

  // The starred button in the touch bar.
  base::scoped_nsobject<NSButton> starredButton_;
}

// Creates and returns a touch bar for tab fullscreen mode.
- (NSTouchBar*)createTabFullscreenTouchBar;

// Updates the starred button in the touch bar.
- (void)updateStarredButton;

// Creates and returns the search button.
- (NSView*)searchTouchBarView;

@end

@implementation BrowserWindowDefaultTouchBar

@synthesize isPageLoading = isPageLoading_;
@synthesize isStarred = isStarred_;
@synthesize canGoBack = canGoBack_;
@synthesize canGoForward = canGoForward_;
@synthesize controller = controller_;
@synthesize browser = browser_;

- (NSTouchBar*)makeTouchBar {
  // When in tab or extension fullscreen, we should show a touch bar containing
  // only items associated with that mode. Since the toolbar is hidden, only
  // the option to exit fullscreen should show up.
  FullscreenController* controller =
      browser_->exclusive_access_manager()->fullscreen_controller();
  if (controller->IsWindowFullscreenForTabOrPending() ||
      controller->IsExtensionFullscreenOrPending()) {
    return [self createTabFullscreenTouchBar];
  }

  base::scoped_nsobject<NSTouchBar> touchBar([[ui::NSTouchBar() alloc] init]);
  [touchBar
      setCustomizationIdentifier:ui::GetTouchBarId(kBrowserWindowTouchBarId)];
  [touchBar setDelegate:self];

  NSMutableArray<NSString*>* customIdentifiers = [NSMutableArray array];
  NSMutableArray<NSString*>* defaultIdentifiers = [NSMutableArray array];

  NSArray<NSString*>* touchBarItems = @[
    kBackTouchId, kForwardTouchId, kReloadOrStopTouchId, kHomeTouchId,
    kSearchTouchId, kStarTouchId, kNewTabTouchId
  ];

  for (NSString* item in touchBarItems) {
    NSString* itemIdentifier =
        ui::GetTouchBarItemId(kBrowserWindowTouchBarId, item);
    [customIdentifiers addObject:itemIdentifier];

    // Don't add the home button if it's not shown in the toolbar.
    if (item == kHomeTouchId && !notificationBridge_->show_home_button())
      continue;

    [defaultIdentifiers addObject:itemIdentifier];
  }

  [customIdentifiers addObject:NSTouchBarItemIdentifierFlexibleSpace];

  [touchBar setDefaultItemIdentifiers:defaultIdentifiers];
  [touchBar setCustomizationAllowedItemIdentifiers:customIdentifiers];

  return touchBar.autorelease();
}

- (NSTouchBarItem*)touchBar:(NSTouchBar*)touchBar
      makeItemForIdentifier:(NSTouchBarItemIdentifier)identifier {
  if (!touchBar)
    return nil;

  if ([identifier hasSuffix:kBackForwardTouchId]) {
    auto* items = @[
      [touchBar itemForIdentifier:ui::GetTouchBarItemId(
                                      kBrowserWindowTouchBarId, kBackTouchId)],
      [touchBar
          itemForIdentifier:ui::GetTouchBarItemId(kBrowserWindowTouchBarId,
                                                  kForwardTouchId)],
    ];
    auto groupItem = [NSGroupTouchBarItem groupItemWithIdentifier:identifier
                                                            items:items];
    [groupItem setCustomizationLabel:
                   l10n_util::GetNSString(
                       IDS_TOUCH_BAR_BACK_FORWARD_CUSTOMIZATION_LABEL)];
    return groupItem;
  }

  base::scoped_nsobject<NSCustomTouchBarItem> touchBarItem(
      [[ui::NSCustomTouchBarItem() alloc] initWithIdentifier:identifier]);
  if ([identifier hasSuffix:kBackTouchId]) {
    auto* button = CreateTouchBarButton(vector_icons::kBackArrowIcon, self,
                                        IDC_BACK, IDS_ACCNAME_BACK);
    [button bind:@"enabled" toObject:self withKeyPath:@"canGoBack" options:nil];
    [touchBarItem setView:button];
    [touchBarItem
        setCustomizationLabel:l10n_util::GetNSString(IDS_ACCNAME_BACK)];
  } else if ([identifier hasSuffix:kForwardTouchId]) {
    auto* button = CreateTouchBarButton(vector_icons::kForwardArrowIcon, self,
                                        IDC_FORWARD, IDS_ACCNAME_FORWARD);
    [button bind:@"enabled"
           toObject:self
        withKeyPath:@"canGoForward"
            options:nil];
    [touchBarItem setView:button];
    [touchBarItem
        setCustomizationLabel:l10n_util::GetNSString(IDS_ACCNAME_FORWARD)];
  } else if ([identifier hasSuffix:kReloadOrStopTouchId]) {
    [self updateReloadStopButton];
    [touchBarItem setView:reloadStopButton_.get()];
    [touchBarItem setCustomizationLabel:
                      l10n_util::GetNSString(
                          IDS_TOUCH_BAR_STOP_RELOAD_CUSTOMIZATION_LABEL)];
  } else if ([identifier hasSuffix:kHomeTouchId]) {
    [touchBarItem setView:CreateTouchBarButton(kNavigateHomeIcon, self,
                                               IDC_HOME, IDS_ACCNAME_HOME)];
    [touchBarItem
        setCustomizationLabel:l10n_util::GetNSString(
                                  IDS_TOUCH_BAR_HOME_CUSTOMIZATION_LABEL)];
  } else if ([identifier hasSuffix:kNewTabTouchId]) {
    [touchBarItem
        setView:CreateTouchBarButton(kNewTabMacTouchbarIcon, self, IDC_NEW_TAB,
                                     IDS_TOOLTIP_NEW_TAB)];
    [touchBarItem
        setCustomizationLabel:l10n_util::GetNSString(
                                  IDS_TOUCH_BAR_NEW_TAB_CUSTOMIZATION_LABEL)];
  } else if ([identifier hasSuffix:kStarTouchId]) {
    [self updateStarredButton];
    [touchBarItem setView:starredButton_.get()];
    [touchBarItem
        setCustomizationLabel:l10n_util::GetNSString(
                                  IDS_TOUCH_BAR_BOOKMARK_CUSTOMIZATION_LABEL)];
  } else if ([identifier hasSuffix:kSearchTouchId]) {
    [touchBarItem setView:[self searchTouchBarView]];
    [touchBarItem setCustomizationLabel:l10n_util::GetNSString(
                                            IDS_TOUCH_BAR_GOOGLE_SEARCH)];
  } else if ([identifier hasSuffix:kFullscreenOriginLabelTouchId]) {
    content::WebContents* contents =
        browser_->tab_strip_model()->GetActiveWebContents();

    if (!contents)
      return nil;

    // Strip the trailing slash.
    url::Parsed parsed;
    base::string16 displayText = url_formatter::FormatUrl(
        contents->GetLastCommittedURL(),
        url_formatter::kFormatUrlOmitTrailingSlashOnBareHostname,
        net::UnescapeRule::SPACES, &parsed, nullptr, nullptr);

    base::scoped_nsobject<NSMutableAttributedString> attributedString(
        [[NSMutableAttributedString alloc]
            initWithString:base::SysUTF16ToNSString(displayText)]);

    if (parsed.path.is_nonempty()) {
      size_t pathIndex = parsed.path.begin;
      [attributedString
          addAttribute:NSForegroundColorAttributeName
                 value:skia::SkColorToSRGBNSColor(kTouchBarUrlPathColor)
                 range:NSMakeRange(pathIndex,
                                   [attributedString length] - pathIndex)];
    }

    [touchBarItem
        setView:[NSTextField labelWithAttributedString:attributedString.get()]];
  } else {
    return nil;
  }

  return touchBarItem.autorelease();
}

- (NSTouchBar*)createTabFullscreenTouchBar {
  base::scoped_nsobject<NSTouchBar> touchBar([[ui::NSTouchBar() alloc] init]);
  [touchBar setDelegate:self];
  [touchBar setDefaultItemIdentifiers:@[ ui::GetTouchBarItemId(
                                          kTabFullscreenTouchBarId,
                                          kFullscreenOriginLabelTouchId) ]];
  return touchBar.autorelease();
}

- (void)setBrowser:(Browser*)browser {
  if (browser_ == browser)
    return;
  browser_ = browser;
  notificationBridge_.reset(
      browser_ ? new TouchBarNotificationBridge(self, browser_) : nullptr);
}

- (void)updateStarredButton {
  const gfx::VectorIcon& icon =
      isStarred_ ? omnibox::kStarActiveIcon : omnibox::kStarIcon;
  SkColor iconColor =
      isStarred_ ? kTouchBarStarActiveColor : kTouchBarDefaultIconColor;
  int tooltipId = isStarred_ ? IDS_TOOLTIP_STARRED : IDS_TOOLTIP_STAR;
  if (!starredButton_) {
    starredButton_.reset([CreateTouchBarButton(
        icon, self, IDC_BOOKMARK_THIS_TAB, tooltipId, iconColor) retain]);
    return;
  }

  [starredButton_ setImage:CreateNSImageFromIcon(icon, iconColor)];
  [starredButton_ setAccessibilityLabel:l10n_util::GetNSString(tooltipId)];
}

- (NSView*)searchTouchBarView {
  TemplateURLService* templateUrlService =
      TemplateURLServiceFactory::GetForProfile(browser_->profile());
  const TemplateURL* defaultProvider =
      templateUrlService->GetDefaultSearchProvider();
  BOOL isGoogle = NO;
  base::string16 title;
  if (defaultProvider) {
    isGoogle =
        defaultProvider->GetEngineType(
            templateUrlService->search_terms_data()) == SEARCH_ENGINE_GOOGLE;

    title = isGoogle ? l10n_util::GetStringUTF16(IDS_TOUCH_BAR_GOOGLE_SEARCH)
                     : l10n_util::GetStringFUTF16(
                           IDS_TOUCH_BAR_SEARCH, defaultProvider->short_name());
  } else {
    title = l10n_util::GetStringUTF16(IDS_TOUCH_BAR_NO_DEFAULT_SEARCH);
  }

  NSImage* image = nil;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (isGoogle) {
    image = NSImageFromImageSkiaWithColorSpace(
        gfx::CreateVectorIcon(kGoogleGLogoIcon, kTouchBarIconSize,
                              gfx::kPlaceholderColor),
        base::mac::GetSRGBColorSpace());
    image = CreateNSImageFromIcon(vector_icons::kSearchIcon);
  }
#endif
  if (!image)
    image = CreateNSImageFromIcon(vector_icons::kSearchIcon);

  NSButton* searchButton =
      [NSButton buttonWithTitle:base::SysUTF16ToNSString(title)
                          image:image
                         target:self
                         action:@selector(executeCommand:)];
  searchButton.imageHugsTitle = YES;
  searchButton.tag = IDC_FOCUS_LOCATION;
  [searchButton.widthAnchor
      constraintGreaterThanOrEqualToConstant:kSearchBtnMinWidth]
      .active = YES;
  [searchButton
      setContentHuggingPriority:1.0
                 forOrientation:NSLayoutConstraintOrientationHorizontal];
  return searchButton;
}

- (void)executeCommand:(id)sender {
  int command = [sender tag];
  ui::LogTouchBarUMA(TouchBarActionFromCommand(command));
  browser_->command_controller()->ExecuteCommand(command);
}

- (void)setIsPageLoading:(BOOL)isPageLoading {
  isPageLoading_ = isPageLoading;
  [self updateReloadStopButton];
}

- (void)setIsStarred:(BOOL)isStarred {
  isStarred_ = isStarred;
  [self updateStarredButton];
}

@end

// Private methods exposed for testing.
@implementation BrowserWindowDefaultTouchBar (ExposedForTesting)

+ (NSString*)reloadOrStopItemIdentifier {
  return ui::GetTouchBarItemId(kBrowserWindowTouchBarId, kReloadOrStopTouchId);
}

+ (NSString*)backItemIdentifier {
  return ui::GetTouchBarItemId(kBrowserWindowTouchBarId, kBackTouchId);
}

+ (NSString*)forwardItemIdentifier {
  return ui::GetTouchBarItemId(kBrowserWindowTouchBarId, kForwardTouchId);
}

+ (NSString*)fullscreenOriginItemIdentifier {
  return ui::GetTouchBarItemId(kTabFullscreenTouchBarId,
                               kFullscreenOriginLabelTouchId);
}

- (void)updateReloadStopButton {
  const gfx::VectorIcon& icon =
      isPageLoading_ ? kNavigateStopIcon : vector_icons::kReloadIcon;
  int commandId = isPageLoading_ ? IDC_STOP : IDC_RELOAD;
  int tooltipId = isPageLoading_ ? IDS_TOOLTIP_STOP : IDS_TOOLTIP_RELOAD;

  if (!reloadStopButton_) {
    reloadStopButton_.reset(
        [CreateTouchBarButton(icon, self, commandId, tooltipId) retain]);
    return;
  }

  [reloadStopButton_
      setImage:CreateNSImageFromIcon(icon, kTouchBarDefaultIconColor)];
  [reloadStopButton_ setTag:commandId];
  [reloadStopButton_ setAccessibilityLabel:l10n_util::GetNSString(tooltipId)];
}

- (NSButton*)reloadStopButton {
  if (!reloadStopButton_)
    [self updateReloadStopButton];

  return reloadStopButton_.get();
}

- (BookmarkTabHelperObserver*)bookmarkTabObserver {
  return notificationBridge_.get();
}

@end
