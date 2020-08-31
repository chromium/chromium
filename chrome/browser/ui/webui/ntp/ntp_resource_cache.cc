// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/webui/app_launcher_login_handler.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/browser/ui/webui/ntp/cookie_controls_handler.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/google/core/common/google_util.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/common/extension_urls.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/template_expressions.h"
#include "ui/base/theme_provider.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#endif

#if defined(OS_MAC)
#include "chrome/browser/platform_util.h"
#endif

using content::BrowserThread;

namespace {

// The URL for the the Learn More page shown on incognito new tab.
const char kLearnMoreIncognitoUrl[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=incognito";
#else
    "https://support.google.com/chrome/?p=incognito";
#endif

// The URL for the Learn More page shown on guest session new tab.
const char kLearnMoreGuestSessionUrl[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=chromebook_guest";
#else
    "https://support.google.com/chrome/?p=ui_guest";
#endif

SkColor GetThemeColor(const ui::ThemeProvider& tp, int id) {
  SkColor color = tp.GetColor(id);
  // If web contents are being inverted because the system is in high-contrast
  // mode, any system theme colors we use must be inverted too to cancel out.
  return ui::NativeTheme::GetInstanceForNativeUi()
                     ->GetPlatformHighContrastColorScheme() ==
                 ui::NativeTheme::PlatformHighContrastColorScheme::kDark
             ? color_utils::InvertColor(color)
             : color;
}

// Get the CSS string for the background position on the new tab page for the
// states when the bar is attached or detached.
std::string GetNewTabBackgroundCSS(const ui::ThemeProvider& theme_provider,
                                   bool bar_attached) {
  // TODO(glen): This is a quick workaround to hide the notused.png image when
  // no image is provided - we don't have time right now to figure out why
  // this is painting as white.
  // http://crbug.com/17593
  if (!theme_provider.HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
    return "-64px";
  }

  int alignment = theme_provider.GetDisplayProperty(
      ThemeProperties::NTP_BACKGROUND_ALIGNMENT);

  if (bar_attached)
    return ThemeProperties::AlignmentToString(alignment);

  if (alignment & ThemeProperties::ALIGN_TOP) {
    // The bar is detached, so we must offset the background by the bar size
    // if it's a top-aligned bar.
    int offset = GetLayoutConstant(BOOKMARK_BAR_NTP_HEIGHT);

    if (alignment & ThemeProperties::ALIGN_LEFT)
      return "left " + base::NumberToString(-offset) + "px";
    else if (alignment & ThemeProperties::ALIGN_RIGHT)
      return "right " + base::NumberToString(-offset) + "px";
    return "center " + base::NumberToString(-offset) + "px";
  }

  return ThemeProperties::AlignmentToString(alignment);
}

// How the background image on the new tab page should be tiled (see tiling
// masks in theme_service.h).
std::string GetNewTabBackgroundTilingCSS(
    const ui::ThemeProvider& theme_provider) {
  int repeat_mode =
      theme_provider.GetDisplayProperty(ThemeProperties::NTP_BACKGROUND_TILING);
  return ThemeProperties::TilingToString(repeat_mode);
}

std::string ReplaceTemplateExpressions(
    const scoped_refptr<base::RefCountedMemory>& bytes,
    const ui::TemplateReplacements& replacements) {
  return ui::ReplaceTemplateExpressions(
      base::StringPiece(reinterpret_cast<const char*>(bytes->front()),
                        bytes->size()),
      replacements);
}

}  // namespace

NTPResourceCache::NTPResourceCache(Profile* profile)
    : profile_(profile), is_swipe_tracking_from_scroll_events_enabled_(false) {
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(profile)));

  base::Closure callback = base::Bind(&NTPResourceCache::OnPreferenceChanged,
                                      base::Unretained(this));

  // Watch for pref changes that cause us to need to invalidate the HTML cache.
  profile_pref_change_registrar_.Init(profile_->GetPrefs());
  profile_pref_change_registrar_.Add(bookmarks::prefs::kShowBookmarkBar,
                                     callback);
  profile_pref_change_registrar_.Add(prefs::kNtpShownPage, callback);
  profile_pref_change_registrar_.Add(prefs::kHideWebStoreIcon, callback);
  profile_pref_change_registrar_.Add(prefs::kCookieControlsMode, callback);

  theme_observer_.Add(ui::NativeTheme::GetInstanceForNativeUi());

  policy_change_registrar_ = std::make_unique<policy::PolicyChangeRegistrar>(
      profile->GetProfilePolicyConnector()->policy_service(),
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
  policy_change_registrar_->Observe(
      policy::key::kBlockThirdPartyCookies,
      base::BindRepeating(&NTPResourceCache::OnPolicyChanged,
                          base::Unretained(this)));
}

NTPResourceCache::~NTPResourceCache() = default;

bool NTPResourceCache::NewTabHTMLNeedsRefresh() {
#if defined(OS_MAC)
  // Invalidate if the current value is different from the cached value.
  bool is_enabled = platform_util::IsSwipeTrackingFromScrollEventsEnabled();
  if (is_enabled != is_swipe_tracking_from_scroll_events_enabled_) {
    is_swipe_tracking_from_scroll_events_enabled_ = is_enabled;
    return true;
  }
#endif
  return false;
}

NTPResourceCache::WindowType NTPResourceCache::GetWindowType(
    Profile* profile, content::RenderProcessHost* render_host) {
  if (profile->IsGuestSession()) {
    return GUEST;
  } else if (render_host) {
    // Sometimes the |profile| is the parent (non-incognito) version of the user
    // so we check the |render_host| if it is provided.
    if (render_host->GetBrowserContext()->IsOffTheRecord())
      return INCOGNITO;
  } else if (profile->IsOffTheRecord()) {
    return INCOGNITO;
  }
  return NORMAL;
}

base::RefCountedMemory* NTPResourceCache::GetNewTabHTML(WindowType win_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (win_type == GUEST) {
    if (!new_tab_guest_html_)
      CreateNewTabGuestHTML();
    return new_tab_guest_html_.get();
  }

  if (win_type == INCOGNITO) {
    if (!new_tab_incognito_html_)
      CreateNewTabIncognitoHTML();
    return new_tab_incognito_html_.get();
  }

  // Refresh the cached HTML if necessary.
  // NOTE: NewTabHTMLNeedsRefresh() must be called every time the new tab
  // HTML is fetched, because it needs to initialize cached values.
  if (NewTabHTMLNeedsRefresh() || !new_tab_html_)
    CreateNewTabHTML();
  return new_tab_html_.get();
}

base::RefCountedMemory* NTPResourceCache::GetNewTabCSS(WindowType win_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Guest mode doesn't have theme-related CSS.
  if (win_type == GUEST)
    return nullptr;

  if (win_type == INCOGNITO) {
    if (!new_tab_incognito_css_)
      CreateNewTabIncognitoCSS();
    return new_tab_incognito_css_.get();
  }

  if (!new_tab_css_)
    CreateNewTabCSS();
  return new_tab_css_.get();
}

void NTPResourceCache::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_THEME_CHANGED, type);

  // Invalidate the cache.
  Invalidate();
}

void NTPResourceCache::OnNativeThemeUpdated(ui::NativeTheme* updated_theme) {
  DCHECK_EQ(updated_theme, ui::NativeTheme::GetInstanceForNativeUi());
  Invalidate();
}

void NTPResourceCache::OnPreferenceChanged() {
  // A change occurred to one of the preferences we care about, so flush the
  // cache.
  new_tab_incognito_html_ = nullptr;
  new_tab_html_ = nullptr;
  new_tab_css_ = nullptr;
}

// TODO(dbeam): why must Invalidate() and OnPreferenceChanged() both exist?
void NTPResourceCache::Invalidate() {
  new_tab_incognito_html_ = nullptr;
  new_tab_html_ = nullptr;
  new_tab_incognito_css_ = nullptr;
  new_tab_css_ = nullptr;
  new_tab_guest_html_ = nullptr;
}

void NTPResourceCache::CreateNewTabIncognitoHTML() {
  ui::TemplateReplacements replacements;
  // Note: there's specific rules in CSS that look for this attribute's content
  // being equal to "true" as a string.
  replacements["bookmarkbarattached"] =
      profile_->GetPrefs()->GetBoolean(bookmarks::prefs::kShowBookmarkBar)
          ? "true"
          : "false";

  // Ensure passing off-the-record profile; |profile_| is not an OTR profile.
  DCHECK(!profile_->IsOffTheRecord());
  DCHECK(profile_->HasPrimaryOTRProfile());
  CookieControlsService* cookie_controls_service =
      CookieControlsServiceFactory::GetForProfile(
          profile_->GetPrimaryOTRProfile());

  replacements["incognitoTabDescription"] =
      l10n_util::GetStringUTF8(IDS_NEW_TAB_OTR_SUBTITLE);
  replacements["incognitoTabHeading"] =
      l10n_util::GetStringUTF8(IDS_NEW_TAB_OTR_TITLE);
  replacements["incognitoTabWarning"] =
      l10n_util::GetStringUTF8(IDS_NEW_TAB_OTR_VISIBLE);
  replacements["learnMore"] =
      l10n_util::GetStringUTF8(IDS_NEW_TAB_OTR_LEARN_MORE_LINK);
  replacements["incognitoTabFeatures"] =
      l10n_util::GetStringUTF8(IDS_NEW_TAB_OTR_NOT_SAVED);
  replacements["learnMoreLink"] = kLearnMoreIncognitoUrl;
  replacements["title"] = l10n_util::GetStringUTF8(IDS_NEW_TAB_TITLE);
  replacements["cookieControlsTitle"] =
      l10n_util::GetStringUTF8(IDS_NEW_TAB_OTR_THIRD_PARTY_COOKIE);
  replacements["cookieControlsDescription"] =
      l10n_util::GetStringUTF8(IDS_NEW_TAB_OTR_THIRD_PARTY_COOKIE_SUBLABEL);
  replacements["cookieControlsToggleChecked"] =
      cookie_controls_service->GetToggleCheckedValue() ? "checked" : "";
  replacements["hideTooltipIcon"] =
      cookie_controls_service->ShouldEnforceCookieControls() ? "" : "hidden";
  replacements["cookieControlsToolTipIcon"] =
      CookieControlsHandler::GetEnforcementIcon(
          cookie_controls_service->GetCookieControlsEnforcement());
  replacements["cookieControlsTooltipText"] = l10n_util::GetStringUTF8(
      IDS_NEW_TAB_OTR_COOKIE_CONTROLS_CONTROLLED_TOOLTIP_TEXT);

  const ui::ThemeProvider& tp =
      ThemeService::GetThemeProviderForProfile(profile_);
  replacements["hasCustomBackground"] =
      tp.HasCustomImage(IDR_THEME_NTP_BACKGROUND) ? "true" : "false";

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &replacements);

  static const base::NoDestructor<scoped_refptr<base::RefCountedMemory>>
      incognito_tab_html(
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              IDR_INCOGNITO_TAB_HTML));
  CHECK(*incognito_tab_html);

  std::string full_html =
      ReplaceTemplateExpressions(*incognito_tab_html, replacements);

  new_tab_incognito_html_ = base::RefCountedString::TakeString(&full_html);
}

void NTPResourceCache::CreateNewTabGuestHTML() {
  base::DictionaryValue localized_strings;
  localized_strings.SetString("title",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
  const char* guest_tab_link = kLearnMoreGuestSessionUrl;
  int guest_tab_idr = IDR_GUEST_TAB_HTML;
  int guest_tab_description_ids = IDS_NEW_TAB_GUEST_SESSION_DESCRIPTION;
  int guest_tab_heading_ids = IDS_NEW_TAB_GUEST_SESSION_HEADING;
  int guest_tab_link_ids = IDS_LEARN_MORE;

#if defined(OS_CHROMEOS)
  guest_tab_idr = IDR_GUEST_SESSION_TAB_HTML;

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();

  if (connector->IsEnterpriseManaged()) {
    localized_strings.SetString("enterpriseInfoVisible", "true");
    localized_strings.SetString("enterpriseLearnMore",
                                l10n_util::GetStringUTF16(IDS_LEARN_MORE));
    localized_strings.SetString("enterpriseInfoHintLink",
                                chrome::kLearnMoreEnterpriseURL);
    base::string16 enterprise_info;
    if (connector->IsCloudManaged()) {
      const std::string enterprise_display_domain =
          connector->GetEnterpriseDisplayDomain();
      enterprise_info = l10n_util::GetStringFUTF16(
          IDS_ASH_ENTERPRISE_DEVICE_MANAGED_BY,
          base::UTF8ToUTF16(enterprise_display_domain));
    } else if (connector->IsActiveDirectoryManaged()) {
      enterprise_info =
          l10n_util::GetStringUTF16(IDS_ASH_ENTERPRISE_DEVICE_MANAGED);
    } else {
      NOTREACHED() << "Unknown management type";
    }
    localized_strings.SetString("enterpriseInfoMessage", enterprise_info);
  } else {
    localized_strings.SetString("enterpriseInfoVisible", "false");
    localized_strings.SetString("enterpriseInfoMessage", "");
    localized_strings.SetString("enterpriseLearnMore", "");
    localized_strings.SetString("enterpriseInfoHintLink", "");
  }
#endif

  localized_strings.SetString("guestTabDescription",
      l10n_util::GetStringUTF16(guest_tab_description_ids));
  localized_strings.SetString("guestTabHeading",
      l10n_util::GetStringUTF16(guest_tab_heading_ids));
  localized_strings.SetString("learnMore",
      l10n_util::GetStringUTF16(guest_tab_link_ids));
  localized_strings.SetString("learnMoreLink", guest_tab_link);

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &localized_strings);

  static const base::NoDestructor<scoped_refptr<base::RefCountedMemory>>
      guest_tab_html(
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              guest_tab_idr));
  CHECK(*guest_tab_html);
  ui::TemplateReplacements replacements;
  ui::TemplateReplacementsFromDictionaryValue(localized_strings, &replacements);
  std::string full_html =
      ReplaceTemplateExpressions(*guest_tab_html, replacements);

  new_tab_guest_html_ = base::RefCountedString::TakeString(&full_html);
}

// TODO(alancutter): Consider moving this utility function up somewhere where it
// can be shared with bookmarks_ui.cc.
// Ampersands are used by menus to determine which characters to use as shortcut
// keys. This functionality is not implemented for NTP.
static base::string16 GetLocalizedString(int message_id) {
  base::string16 result = l10n_util::GetStringUTF16(message_id);
  base::Erase(result, '&');
  return result;
}

void NTPResourceCache::CreateNewTabHTML() {
  // TODO(estade): these strings should be defined in their relevant handlers
  // (in GetLocalizedValues) and should have more legible names.
  // Show the profile name in the title and most visited labels if the current
  // profile is not the default.
  PrefService* prefs = profile_->GetPrefs();
  base::DictionaryValue load_time_data;
  load_time_data.SetString(
      "bookmarkbarattached",
      prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar) ? "true" : "false");
  load_time_data.SetString("title", GetLocalizedString(IDS_NEW_TAB_TITLE));
  load_time_data.SetString("webStoreTitle",
                           GetLocalizedString(IDS_EXTENSION_WEB_STORE_TITLE));
  load_time_data.SetString(
      "webStoreTitleShort",
      GetLocalizedString(IDS_EXTENSION_WEB_STORE_TITLE_SHORT));
  load_time_data.SetString("attributionintro",
                           GetLocalizedString(IDS_NEW_TAB_ATTRIBUTION_INTRO));
  load_time_data.SetString("appuninstall",
                           GetLocalizedString(IDS_EXTENSIONS_UNINSTALL));
  load_time_data.SetString("appoptions",
                           GetLocalizedString(IDS_NEW_TAB_APP_OPTIONS));
  load_time_data.SetString("appdetails",
                           GetLocalizedString(IDS_NEW_TAB_APP_DETAILS));
  load_time_data.SetString("appinfodialog",
                           GetLocalizedString(IDS_APP_CONTEXT_MENU_SHOW_INFO));
  load_time_data.SetString("appcreateshortcut",
                           GetLocalizedString(IDS_NEW_TAB_APP_CREATE_SHORTCUT));
  load_time_data.SetString("appinstalllocally",
                           GetLocalizedString(IDS_NEW_TAB_APP_INSTALL_LOCALLY));
  load_time_data.SetString("appDefaultPageName",
                           GetLocalizedString(IDS_APP_DEFAULT_PAGE_NAME));
  load_time_data.SetString(
      "applaunchtypepinned",
      GetLocalizedString(IDS_APP_CONTEXT_MENU_OPEN_PINNED));
  load_time_data.SetString(
      "applaunchtyperegular",
      GetLocalizedString(IDS_APP_CONTEXT_MENU_OPEN_REGULAR));
  load_time_data.SetString(
      "applaunchtypewindow",
      GetLocalizedString(IDS_APP_CONTEXT_MENU_OPEN_WINDOW));
  load_time_data.SetString(
      "applaunchtypefullscreen",
      GetLocalizedString(IDS_APP_CONTEXT_MENU_OPEN_FULLSCREEN));
  load_time_data.SetString(
      "syncpromotext", GetLocalizedString(IDS_SYNC_START_SYNC_BUTTON_LABEL));
  load_time_data.SetString("syncLinkText",
                           GetLocalizedString(IDS_SYNC_ADVANCED_OPTIONS));
  load_time_data.SetBoolean("shouldShowSyncLogin",
                            AppLauncherLoginHandler::ShouldShow(profile_));
  load_time_data.SetString("learnMore", GetLocalizedString(IDS_LEARN_MORE));
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  load_time_data.SetString(
      "webStoreLink", google_util::AppendGoogleLocaleParam(
                          extension_urls::GetWebstoreLaunchURL(), app_locale)
                          .spec());
  load_time_data.SetString(
      "appInstallHintText",
      GetLocalizedString(IDS_NEW_TAB_APP_INSTALL_HINT_LABEL));
  load_time_data.SetString("learn_more", GetLocalizedString(IDS_LEARN_MORE));
  load_time_data.SetString(
      "tile_grid_screenreader_accessible_description",
      GetLocalizedString(IDS_NEW_TAB_TILE_GRID_ACCESSIBLE_DESCRIPTION));
  load_time_data.SetString(
      "page_switcher_change_title",
      GetLocalizedString(IDS_NEW_TAB_PAGE_SWITCHER_CHANGE_TITLE));
  load_time_data.SetString(
      "page_switcher_same_title",
      GetLocalizedString(IDS_NEW_TAB_PAGE_SWITCHER_SAME_TITLE));
  // On Mac OS X 10.7+, horizontal scrolling can be treated as a back or
  // forward gesture. Pass through a flag that indicates whether or not that
  // feature is enabled.
  load_time_data.SetBoolean("isSwipeTrackingFromScrollEventsEnabled",
                            is_swipe_tracking_from_scroll_events_enabled_);

  load_time_data.SetBoolean("showWebStoreIcon",
                            !prefs->GetBoolean(prefs::kHideWebStoreIcon));

  load_time_data.SetBoolean("canShowAppInfoDialog",
                            CanPlatformShowAppInfoDialog());

  AppLauncherHandler::GetLocalizedValues(profile_, &load_time_data);

  webui::SetLoadTimeDataDefaults(app_locale, &load_time_data);

  // Control fade and resize animations.
  load_time_data.SetBoolean("anim",
                            gfx::Animation::ShouldRenderRichAnimation());

  load_time_data.SetBoolean(
      "isUserSignedIn",
      IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount());

  // Load the new tab page template and localize it.
  static const base::NoDestructor<scoped_refptr<base::RefCountedMemory>>
      new_tab_html(
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              IDR_NEW_TAB_4_HTML));
  CHECK(*new_tab_html);
  std::string full_html = webui::GetI18nTemplateHtml(
      base::StringPiece(reinterpret_cast<const char*>((*new_tab_html)->front()),
                        (*new_tab_html)->size()),
      &load_time_data);
  new_tab_html_ = base::RefCountedString::TakeString(&full_html);
}

void NTPResourceCache::CreateNewTabIncognitoCSS() {
  const ui::ThemeProvider& tp = ThemeService::GetThemeProviderForProfile(
      profile_->GetPrimaryOTRProfile());

  // Generate the replacements.
  ui::TemplateReplacements substitutions;

  // Cache-buster for background.
  substitutions["themeId"] =
      profile_->GetPrefs()->GetString(prefs::kCurrentThemeID);

  // Colors.
  substitutions["colorBackground"] = color_utils::SkColorToRgbaString(
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_BACKGROUND));
  substitutions["backgroundBarDetached"] = GetNewTabBackgroundCSS(tp, false);
  substitutions["backgroundBarAttached"] = GetNewTabBackgroundCSS(tp, true);
  substitutions["backgroundTiling"] = GetNewTabBackgroundTilingCSS(tp);

  // Get our template.
  static const base::NoDestructor<scoped_refptr<base::RefCountedMemory>>
      new_tab_theme_css(
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              IDR_INCOGNITO_TAB_THEME_CSS));
  CHECK(*new_tab_theme_css);

  // Create the string from our template and the replacements.
  std::string full_css =
      ReplaceTemplateExpressions(*new_tab_theme_css, substitutions);

  new_tab_incognito_css_ = base::RefCountedString::TakeString(&full_css);
}

void NTPResourceCache::CreateNewTabCSS() {
  const ui::ThemeProvider& tp =
      ThemeService::GetThemeProviderForProfile(profile_);

  // Get our theme colors.
  SkColor color_background =
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_BACKGROUND);
  SkColor color_text = GetThemeColor(tp, ThemeProperties::COLOR_NTP_TEXT);
  SkColor color_text_light =
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_TEXT_LIGHT);

  SkColor color_header =
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_HEADER);
  // Generate a lighter color for the header gradients.
  color_utils::HSL header_lighter;
  color_utils::SkColorToHSL(color_header, &header_lighter);
  header_lighter.l += (1 - header_lighter.l) * 0.33;

  // Generate section border color from the header color. See
  // BookmarkBarView::Paint for how we do this for the bookmark bar
  // borders.
  SkColor color_section_border =
      SkColorSetARGB(80,
                     SkColorGetR(color_header),
                     SkColorGetG(color_header),
                     SkColorGetB(color_header));

  // Generate the replacements.
  ui::TemplateReplacements substitutions;

  // Cache-buster for background.
  substitutions["themeId"] =
      profile_->GetPrefs()->GetString(prefs::kCurrentThemeID);

  // Colors.
  substitutions["colorBackground"] =
      color_utils::SkColorToRgbaString(color_background);
  substitutions["colorLink"] = color_utils::SkColorToRgbString(
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_LINK));
  substitutions["backgroundBarDetached"] = GetNewTabBackgroundCSS(tp, false);
  substitutions["backgroundBarAttached"] = GetNewTabBackgroundCSS(tp, true);
  substitutions["backgroundTiling"] = GetNewTabBackgroundTilingCSS(tp);
  substitutions["colorTextRgba"] = color_utils::SkColorToRgbaString(color_text);
  substitutions["colorTextLight"] =
      color_utils::SkColorToRgbaString(color_text_light);
  substitutions["colorSectionBorder"] =
      color_utils::SkColorToRgbString(color_section_border);
  substitutions["colorText"] = color_utils::SkColorToRgbString(color_text);

  // For themes that right-align the background, we flip the attribution to the
  // left to avoid conflicts.
  int alignment =
      tp.GetDisplayProperty(ThemeProperties::NTP_BACKGROUND_ALIGNMENT);
  if (alignment & ThemeProperties::ALIGN_RIGHT) {
    substitutions["leftAlignAttribution"] = "0";
    substitutions["rightAlignAttribution"] = "auto";
    substitutions["textAlignAttribution"] = "right";
  } else {
    substitutions["leftAlignAttribution"] = "auto";
    substitutions["rightAlignAttribution"] = "0";
    substitutions["textAlignAttribution"] = "left";
  }

  substitutions["displayAttribution"] =
      tp.HasCustomImage(IDR_THEME_NTP_ATTRIBUTION) ? "inline" : "none";

  // Get our template.
  static const base::NoDestructor<scoped_refptr<base::RefCountedMemory>>
      new_tab_theme_css(
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              IDR_NEW_TAB_4_THEME_CSS));
  CHECK(*new_tab_theme_css);

  // Create the string from our template and the replacements.
  std::string css_string =
      ReplaceTemplateExpressions(*new_tab_theme_css, substitutions);
  new_tab_css_ = base::RefCountedString::TakeString(&css_string);
}

void NTPResourceCache::OnPolicyChanged(const base::Value* previous,
                                       const base::Value* current) {
  new_tab_incognito_html_ = nullptr;
}
