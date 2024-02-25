// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

#import "chrome/browser/ui/cocoa/touchbar/browser_window_default_touch_bar.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/mac/mac_util.h"
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
#include "chrome/browser/ui/fullscreen_util_mac.h"
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
  return NSImageFromImageSkia(
      gfx::CreateVectorIcon(icon, kTouchBarIconSize, color));
}

// Creates an NSButton for the touch bar using an existing NSImage.
NSButton* CreateTouchBarButtonWithImage(NSImage* image,
                                        BrowserWindowDefaultTouchBar* owner,
                                        int command,
                                        int tooltip_id) {
  NSButton* button = [NSButton buttonWithImage:image
                                        target:owner
                                        action:@selector(executeCommand:)];
  button.tag = command;
  button.accessibilityTitle = l10n_util::GetNSString(tooltip_id);
  return button;
}

// Creates an NSButton for the touch bar using a vector icon.
NSButton* CreateTouchBarButton(const gfx::VectorIcon& icon,
                               BrowserWindowDefaultTouchBar* owner,
                               int command,
                               int tooltip_id,
                               SkColor color = kTouchBarDefaultIconColor) {
  return CreateTouchBarButtonWithImage(CreateNSImageFromIcon(icon, color),
                                       owner, command, tooltip_id);
}

// A class registered for C++ notifications. This is used to detect changes in
// the profile preferences and the back/forward commands.
class TouchBarNotificationBridge : public CommandObserver,
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

  TouchBarNotificationBridge(const TouchBarNotificationBridge&) = delete;
  TouchBarNotificationBridge& operator=(const TouchBarNotificationBridge&) =
      delete;

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
  BrowserWindowDefaultTouchBar* __weak owner_;
  raw_ptr<Browser> browser_;             // Weak.
  raw_ptr<content::WebContents> contents_;  // Weak.

  // Used to monitor the optional home button pref.
  BooleanPrefMember show_home_button_;

  PrefChangeRegistrar profile_pref_registrar_;
};

}  // namespace

@interface BrowserWindowDefaultTouchBar () {
  // Used to receive and handle notifications.
  std::unique_ptr<TouchBarNotificationBridge> _notificationBridge;

  // The stop/reload button in the touch bar.
  NSButton* __strong _reloadStopButton;

  // The starred button in the touch bar.
  NSButton* __strong _starredButton;

  // The search button in the touch bar.
  NSButton* __strong _searchButton;

  // The last created BrowserWindowDefaultTouchBar (cached until it needs a
  // rebuild).
  NSTouchBar* __strong _touchBar;

  // The existence of the Home button in the Touch Bar.
  bool _touchBarHasHomeButton;
}

// Creates and returns a touch bar for tab non-fullscreen mode.
- (NSTouchBar*)createTabTouchBar;

// Creates and returns a touch bar for tab fullscreen mode.
- (NSTouchBar*)createTabFullscreenTouchBar;

// Updates the starred button in the touch bar.
- (void)updateStarredButton;

// Updates the reload / stop button in the touch bar.
- (void)updateReloadStopButton;

// Updates the search button in the touch bar.
- (void)updateSearchTouchBarButton;

@end

@implementation BrowserWindowDefaultTouchBar

@synthesize isPageLoading = _isPageLoading;
@synthesize isStarred = _isStarred;
@synthesize canGoBack = _canGoBack;
@synthesize canGoForward = _canGoForward;
@synthesize controller = _controller;
@synthesize browser = _browser;

- (NSTouchBar*)makeTouchBar {
  // When in tab or extension fullscreen, we should show a touch bar containing
  // only items associated with that mode. Since the toolbar is hidden, only
  // the option to exit fullscreen should show up.
  if (fullscreen_utils::IsInContentFullscreen(_browser)) {
    return [self createTabFullscreenTouchBar];
  }

  return [self createTabTouchBar];
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

  NSCustomTouchBarItem* touchBarItem =
      [[NSCustomTouchBarItem alloc] initWithIdentifier:identifier];
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
    [touchBarItem setView:_reloadStopButton];
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
    [touchBarItem setView:_starredButton];
    [touchBarItem
        setCustomizationLabel:l10n_util::GetNSString(
                                  IDS_TOUCH_BAR_BOOKMARK_CUSTOMIZATION_LABEL)];
  } else if ([identifier hasSuffix:kSearchTouchId]) {
    [self updateSearchTouchBarButton];
    [touchBarItem setView:_searchButton];
    [touchBarItem setCustomizationLabel:l10n_util::GetNSString(
                                            IDS_TOUCH_BAR_GOOGLE_SEARCH)];
  } else if ([identifier hasSuffix:kFullscreenOriginLabelTouchId]) {
    content::WebContents* contents =
        _browser->tab_strip_model()->GetActiveWebContents();

    if (!contents)
      return nil;

    // Strip the trailing slash.
    url::Parsed parsed;
    std::u16string displayText = url_formatter::FormatUrl(
        contents->GetLastCommittedURL(),
        url_formatter::kFormatUrlOmitTrailingSlashOnBareHostname,
        base::UnescapeRule::SPACES, &parsed, nullptr, nullptr);

    NSMutableAttributedString* attributedString =
        [[NSMutableAttributedString alloc]
            initWithString:base::SysUTF16ToNSString(displayText)];

    if (parsed.path.is_nonempty()) {
      size_t pathIndex = parsed.path.begin;
      [attributedString
          addAttribute:NSForegroundColorAttributeName
                 value:skia::SkColorToSRGBNSColor(kTouchBarUrlPathColor)
                 range:NSMakeRange(pathIndex,
                                   attributedString.length - pathIndex)];
    }

    [touchBarItem
        setView:[NSTextField labelWithAttributedString:attributedString]];
    [touchBarItem
        setCustomizationLabel:l10n_util::GetNSString(
                                  IDS_TOUCH_BAR_URL_CUSTOMIZATION_LABEL)];
  } else {
    return nil;
  }

  return touchBarItem;
}

- (NSTouchBar*)createTabTouchBar {
  [self updateSearchTouchBarButton];
  bool showHomeButton = _notificationBridge->show_home_button();

  if (!_touchBar || _touchBarHasHomeButton != showHomeButton) {
    _touchBar = [[NSTouchBar alloc] init];
    [_touchBar
        setCustomizationIdentifier:ui::GetTouchBarId(kBrowserWindowTouchBarId)];
    [_touchBar setDelegate:self];

    NSMutableArray<NSString*>* customizationIdentifiers =
        [NSMutableArray array];
    NSMutableArray<NSString*>* defaultIdentifiers = [NSMutableArray array];

    NSArray<NSString*>* touchBarItemIdentifiers = @[
      kBackTouchId, kForwardTouchId, kReloadOrStopTouchId, kHomeTouchId,
      kSearchTouchId, kStarTouchId, kNewTabTouchId
    ];

    for (NSString* itemIdentifier in touchBarItemIdentifiers) {
      NSString* fullIdentifier =
          ui::GetTouchBarItemId(kBrowserWindowTouchBarId, itemIdentifier);
      [customizationIdentifiers addObject:fullIdentifier];

      // Don't add the home button if it's not shown in the toolbar.
      if (itemIdentifier == kHomeTouchId && !showHomeButton) {
        continue;
      }

      [defaultIdentifiers addObject:fullIdentifier];
    }

    [customizationIdentifiers addObject:NSTouchBarItemIdentifierFlexibleSpace];

    [_touchBar setDefaultItemIdentifiers:defaultIdentifiers];
    [_touchBar setCustomizationAllowedItemIdentifiers:customizationIdentifiers];

    _touchBarHasHomeButton = showHomeButton;
  }

  return _touchBar;
}

- (NSTouchBar*)createTabFullscreenTouchBar {
  NSTouchBar* touchBar = [[NSTouchBar alloc] init];
  [touchBar
      setCustomizationIdentifier:ui::GetTouchBarId(kTabFullscreenTouchBarId)];
  [touchBar setDelegate:self];

  NSArray<NSString*>* touchBarItems = @[ ui::GetTouchBarItemId(
      kTabFullscreenTouchBarId, kFullscreenOriginLabelTouchId) ];

  [touchBar setDefaultItemIdentifiers:touchBarItems];
  [touchBar setCustomizationAllowedItemIdentifiers:touchBarItems];

  return touchBar;
}

- (void)setBrowser:(Browser*)browser {
  if (_browser == browser)
    return;
  _browser = browser;
  _notificationBridge.reset(
      _browser ? new TouchBarNotificationBridge(self, _browser) : nullptr);
}

- (void)updateStarredButton {
  NSImage* image = _isStarred ? [BrowserWindowDefaultTouchBar starActiveIcon]
                              : [BrowserWindowDefaultTouchBar starDefaultIcon];
  int tooltipId = _isStarred ? IDS_TOOLTIP_STARRED : IDS_TOOLTIP_STAR;

  if (!_starredButton) {
    _starredButton = CreateTouchBarButtonWithImage(
        image, self, IDC_BOOKMARK_THIS_TAB, tooltipId);
    return;
  }

  if ([_starredButton image] == image) {
    return;
  }

  [_starredButton setImage:image];
  [_starredButton setAccessibilityLabel:l10n_util::GetNSString(tooltipId)];
}

- (void)updateReloadStopButton {
  NSImage* image = _isPageLoading
                       ? [BrowserWindowDefaultTouchBar navigateStopIcon]
                       : [BrowserWindowDefaultTouchBar reloadIcon];
  int commandId = _isPageLoading ? IDC_STOP : IDC_RELOAD;
  int tooltipId = _isPageLoading ? IDS_TOOLTIP_STOP : IDS_TOOLTIP_RELOAD;

  if (!_reloadStopButton) {
    _reloadStopButton =
        CreateTouchBarButtonWithImage(image, self, commandId, tooltipId);
    return;
  }

  if ([_reloadStopButton tag] == commandId) {
    return;
  }

  [_reloadStopButton setImage:image];
  [_reloadStopButton setTag:commandId];
  [_reloadStopButton setAccessibilityLabel:l10n_util::GetNSString(tooltipId)];
}

- (void)updateSearchTouchBarButton {
  TemplateURLService* templateUrlService =
      TemplateURLServiceFactory::GetForProfile(_browser->profile());
  const TemplateURL* defaultProvider =
      templateUrlService->GetDefaultSearchProvider();
  BOOL isGoogle = NO;
  std::u16string title;
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

  NSString* buttonTitle = base::SysUTF16ToNSString(title);

  if ([buttonTitle isEqualToString:[_searchButton title]]) {
    return;
  }

  NSImage* image = nil;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (isGoogle) {
    image = NSImageFromImageSkia(
        gfx::CreateVectorIcon(vector_icons::kGoogleGLogoIcon, kTouchBarIconSize,
                              gfx::kPlaceholderColor));
  } else {
    image = CreateNSImageFromIcon(vector_icons::kSearchIcon);
  }
#endif
  if (!image)
    image = CreateNSImageFromIcon(vector_icons::kSearchIcon);

  if (!_searchButton) {
    _searchButton = [NSButton buttonWithTitle:buttonTitle
                                        image:image
                                       target:self
                                       action:@selector(executeCommand:)];
    _searchButton.imageHugsTitle = YES;
    _searchButton.tag = IDC_FOCUS_LOCATION;
    [_searchButton.widthAnchor
        constraintGreaterThanOrEqualToConstant:kSearchBtnMinWidth]
        .active = YES;
    [_searchButton
        setContentHuggingPriority:1.0
                   forOrientation:NSLayoutConstraintOrientationHorizontal];
  } else {
    [_searchButton setTitle:buttonTitle];
    [_searchButton setImage:image];
  }
}

- (void)executeCommand:(id)sender {
  int command = [sender tag];
  _browser->command_controller()->ExecuteCommand(command);
}

- (void)setIsPageLoading:(BOOL)isPageLoading {
  _isPageLoading = isPageLoading;
  [self updateReloadStopButton];
}

- (void)setIsStarred:(BOOL)isStarred {
  _isStarred = isStarred;
  [self updateStarredButton];
}

@end

// Private methods exposed for testing.
@implementation BrowserWindowDefaultTouchBar (ExposedForTesting)

+ (NSString*)reloadOrStopItemIdentifier {
  return ui::GetTouchBarItemId(kBrowserWindowTouchBarId, kReloadOrStopTouchId);
}

+ (NSString*)bookmarkStarItemIdentifier {
  return ui::GetTouchBarItemId(kBrowserWindowTouchBarId, kStarTouchId);
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

+ (NSImage*)starDefaultIcon {
  static __strong NSImage* starDefaultIcon =
      CreateNSImageFromIcon(omnibox::kStarIcon, kTouchBarDefaultIconColor);
  return starDefaultIcon;
}

+ (NSString*)homeItemIdentifier {
  return ui::GetTouchBarItemId(kBrowserWindowTouchBarId, kHomeTouchId);
}

+ (NSImage*)starActiveIcon {
  static __strong NSImage* starActiveIcon = []() {
    return CreateNSImageFromIcon(omnibox::kStarActiveIcon,
                                 kTouchBarStarActiveColor);
  }();
  return starActiveIcon;
}

+ (NSImage*)navigateStopIcon {
  static __strong NSImage* navigateStopIcon =
      CreateNSImageFromIcon(kNavigateStopIcon);
  return navigateStopIcon;
}

+ (NSImage*)reloadIcon {
  static __strong NSImage* reloadIcon =
      CreateNSImageFromIcon(vector_icons::kReloadIcon);
  return reloadIcon;
}

- (NSButton*)searchButton {
  return _searchButton;
}

- (BookmarkTabHelperObserver*)bookmarkTabObserver {
  return _notificationBridge.get();
}

@end
