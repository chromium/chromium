// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/webui/ntp/cookie_controls_handler.h"
#include "chrome/browser/ui/webui/webui_util_desktop.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/template_expressions.h"
#include "ui/base/theme_provider.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/chromeos/devicetype_utils.h"
#endif

using content::BrowserThread;

namespace {

// The URL for the the Learn More page shown on incognito new tab.
const char kLearnMoreIncognitoUrl[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=incognito";
#else
    "https://support.google.com/chrome/?p=incognito";
#endif

// The URL for the Learn More page shown on guest session new tab.
const char kLearnMoreGuestSessionUrl[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=chromebook_guest";
#else
    "https://support.google.com/chrome/?p=ui_guest";
#endif

std::string ReplaceTemplateExpressions(
    const scoped_refptr<base::RefCountedMemory>& bytes,
    const ui::TemplateReplacements& replacements) {
  return ui::ReplaceTemplateExpressions(base::as_string_view(*bytes),
                                        replacements);
}

}  // namespace

SkColor GetThemeColor(const ui::NativeTheme* native_theme,
                      const ui::ColorProvider& cp,
                      int id) {
  SkColor color = cp.GetColor(id);
  // If web contents are being inverted because the system is in high-contrast
  // mode, any system theme colors we use must be inverted too to cancel out.
  return native_theme->GetPlatformHighContrastColorScheme() ==
                 ui::NativeTheme::PlatformHighContrastColorScheme::kDark
             ? color_utils::InvertColor(color)
             : color;
}

// Get the CSS string for the background position on the new tab page.
std::string GetNewTabBackgroundPositionCSS(
    const ui::ThemeProvider& theme_provider) {
  // TODO(glen): This is a quick workaround to hide the notused.png image when
  // no image is provided - we don't have time right now to figure out why
  // this is painting as white.
  // http://crbug.com/17593
  if (!theme_provider.HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
    return "-64px";
  }

  return ThemeProperties::AlignmentToString(theme_provider.GetDisplayProperty(
      ThemeProperties::NTP_BACKGROUND_ALIGNMENT));
}

// How the background image on the new tab page should be tiled (see tiling
// masks in theme_service.h).
std::string GetNewTabBackgroundTilingCSS(
    const ui::ThemeProvider& theme_provider) {
  int repeat_mode =
      theme_provider.GetDisplayProperty(ThemeProperties::NTP_BACKGROUND_TILING);
  return ThemeProperties::TilingToString(repeat_mode);
}

NTPResourceCache::NTPResourceCache(Profile* profile)
    : profile_(profile), is_swipe_tracking_from_scroll_events_enabled_(false) {
  ThemeServiceFactory::GetForProfile(profile_)->AddObserver(this);

  base::RepeatingClosure callback = base::BindRepeating(
      &NTPResourceCache::OnPreferenceChanged, base::Unretained(this));

  // Watch for pref changes that cause us to need to invalidate the HTML cache.
  profile_pref_change_registrar_.Init(profile_->GetPrefs());
  profile_pref_change_registrar_.Add(prefs::kCookieControlsMode, callback);

  // TODO(crbug.com/40677117): Remove the global accessor to NativeTheme.
  theme_observation_.Observe(ui::NativeTheme::GetInstanceForNativeUi());

  policy_change_registrar_ = std::make_unique<policy::PolicyChangeRegistrar>(
      profile->GetProfilePolicyConnector()->policy_service(),
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
  policy_change_registrar_->Observe(
      policy::key::kBlockThirdPartyCookies,
      base::BindRepeating(&NTPResourceCache::OnPolicyChanged,
                          base::Unretained(this)));
}

NTPResourceCache::~NTPResourceCache() = default;

NTPResourceCache::WindowType NTPResourceCache::GetWindowType(Profile* profile) {
  if (profile->IsGuestSession())
    return GUEST;
  if (profile->IsIncognitoProfile())
    return INCOGNITO;
  if (profile->IsOffTheRecord())
    return NON_PRIMARY_OTR;

  return NORMAL;
}

base::RefCountedMemory* NTPResourceCache::GetNewTabGuestHTML() {
  if (!new_tab_guest_html_)
    CreateNewTabGuestHTML();

  return new_tab_guest_html_.get();
}

base::RefCountedMemory* NTPResourceCache::GetNewTabHTML(
    WindowType win_type,
    const content::WebContents::Getter& wc_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  switch (win_type) {
    case GUEST:
      return GetNewTabGuestHTML();

    case INCOGNITO:
      if (!new_tab_incognito_html_)
        CreateNewTabIncognitoHTML(wc_getter);
      return new_tab_incognito_html_.get();

    case NON_PRIMARY_OTR:
      if (!new_tab_non_primary_otr_html_) {
        new_tab_non_primary_otr_html_ =
            base::MakeRefCounted<base::RefCountedString>(std::string());
      }
      return new_tab_non_primary_otr_html_.get();

    case NORMAL:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

base::RefCountedMemory* NTPResourceCache::GetNewTabCSS(
    WindowType win_type,
    const content::WebContents::Getter& wc_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Guest mode doesn't have theme-related CSS.
  if (win_type == GUEST)
    return nullptr;

  // Returns the cached CSS if it exists.
  // The cache will be invalidated when the theme of |wc_getter| changes.
  if (win_type == INCOGNITO) {
    if (!new_tab_incognito_css_)
      CreateNewTabIncognitoCSS(wc_getter);
    return new_tab_incognito_css_.get();
  }

  if (!new_tab_css_)
    CreateNewTabCSS(wc_getter);
  return new_tab_css_.get();
}

void NTPResourceCache::OnThemeChanged() {
  Invalidate();
}

void NTPResourceCache::Shutdown() {
  ThemeServiceFactory::GetForProfile(profile_)->RemoveObserver(this);
}

void NTPResourceCache::OnNativeThemeUpdated(ui::NativeTheme* updated_theme) {
  // TODO(crbug.com/40677117): Remove the global accessor to NativeTheme.
  DCHECK_EQ(updated_theme, ui::NativeTheme::GetInstanceForNativeUi());
  Invalidate();
}

void NTPResourceCache::OnPreferenceChanged() {
  // A change occurred to one of the preferences we care about, so flush the
  // cache.
  new_tab_incognito_html_ = nullptr;
  new_tab_css_ = nullptr;
}

// TODO(dbeam): why must Invalidate() and OnPreferenceChanged() both exist?
void NTPResourceCache::Invalidate() {
  new_tab_incognito_html_ = nullptr;
  new_tab_incognito_css_ = nullptr;
  new_tab_css_ = nullptr;
  new_tab_guest_html_ = nullptr;
}

void NTPResourceCache::CreateNewTabIncognitoHTML(
    const content::WebContents::Getter& wc_getter) {
  ui::TemplateReplacements replacements;
  base::Value::Dict localized_strings;

  // Ensure passing off-the-record profile; |profile_| is not an OTR profile.
  DCHECK(!profile_->IsOffTheRecord());
  DCHECK(profile_->HasAnyOffTheRecordProfile());

  // Cookie controls service returns the same result for all off-the-record
  // profiles, so it doesn't matter which of them we use.
  Profile* incognito_profile = profile_->GetAllOffTheRecordProfiles()[0];
  CookieControlsService* cookie_controls_service =
      CookieControlsServiceFactory::GetForProfile(incognito_profile);

  replacements["incognitoTabDescription"] =
      l10n_util::GetStringUTF8(IDS_NEW_TAB_OTR_SUBTITLE_WITH_READING_LIST);

  privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings =
      TrackingProtectionSettingsFactory::GetForProfile(incognito_profile);
  bool is_tracking_protection_3pcd_enabled =
      tracking_protection_settings->IsTrackingProtection3pcdEnabled();

  replacements["incognitoTabHeading"] =
      l10n_util::GetStringUTF8(IDS_NEW_TAB_OTR_TITLE);
  replacements["incognitoTabWarning"] =
      l10n_util::GetStringUTF8(IDS_NEW_TAB_OTR_VISIBLE);
  replacements["incognitoTabFeatures"] =
      l10n_util::GetStringUTF8(IDS_NEW_TAB_OTR_NOT_SAVED);
  replacements["learnMore"] =
      l10n_util::GetStringUTF8(IDS_NEW_TAB_OTR_LEARN_MORE_LINK);
  replacements["cookieControlsTitle"] =
      l10n_util::GetStringUTF8(IDS_NEW_TAB_OTR_THIRD_PARTY_COOKIE);

  replacements["learnMoreLink"] = kLearnMoreIncognitoUrl;
  replacements["learnMoreA11yLabel"] = l10n_util::GetStringUTF8(
      IDS_INCOGNITO_TAB_LEARN_MORE_ACCESSIBILITY_LABEL);
  replacements["title"] = l10n_util::GetStringUTF8(IDS_NEW_INCOGNITO_TAB_TITLE);

  if (is_tracking_protection_3pcd_enabled) {
    replacements["hideBlockCookiesToggle"] = "hidden";
    replacements["hideTooltipIcon"] = "hidden";

    // Overwrite the cookies control title and description if 3pcd enabled.
    replacements["cookieControlsTitle"] =
        l10n_util::GetStringUTF8(IDS_NEW_TAB_OTR_THIRD_PARTY_BLOCKED_COOKIE);
    localized_strings.Set(
        "cookieControlsDescription",
        l10n_util::GetStringFUTF16(
            IDS_NEW_TAB_OTR_THIRD_PARTY_BLOCKED_COOKIE_SUBLABEL,
            chrome::kUserBypassHelpCenterURL,
            l10n_util::GetStringUTF16(
                IDS_NEW_TAB_OPENS_HC_ARTICLE_IN_NEW_TAB)));

  } else {
    replacements["hideBlockCookiesToggle"] = "";
    replacements["hideTooltipIcon"] =
        cookie_controls_service->ShouldEnforceCookieControls() ? "" : "hidden";
    replacements["cookieControlsDescription"] =
        l10n_util::GetStringUTF8(IDS_NEW_TAB_OTR_THIRD_PARTY_COOKIE_SUBLABEL);
  }

  replacements["cookieControlsToggleChecked"] =
      cookie_controls_service->GetToggleCheckedValue() ? "checked" : "";
  replacements["cookieControlsToolTipIcon"] =
      CookieControlsHandler::GetEnforcementIcon(
          cookie_controls_service->GetCookieControlsEnforcement());
  replacements["cookieControlsTooltipText"] = l10n_util::GetStringUTF8(
      IDS_NEW_TAB_OTR_COOKIE_CONTROLS_CONTROLLED_TOOLTIP_TEXT);

  const ui::ThemeProvider& tp =
      ThemeService::GetThemeProviderForProfile(incognito_profile);

  replacements["hasCustomBackground"] =
      tp.HasCustomImage(IDR_THEME_NTP_BACKGROUND) ? "true" : "false";

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &replacements);

  static const base::NoDestructor<scoped_refptr<base::RefCountedMemory>>
      incognito_tab_html(
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              IDR_INCOGNITO_TAB_HTML));
  CHECK(*incognito_tab_html);
  ui::TemplateReplacementsFromDictionaryValue(localized_strings, &replacements);
  new_tab_incognito_html_ = base::MakeRefCounted<base::RefCountedString>(
      ReplaceTemplateExpressions(*incognito_tab_html, replacements));
}

void NTPResourceCache::CreateNewTabGuestHTML() {
  base::Value::Dict localized_strings;
  localized_strings.Set("title", l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
  const char* guest_tab_link = kLearnMoreGuestSessionUrl;
  int guest_tab_idr = IDR_GUEST_TAB_HTML;
  int guest_tab_description_ids = IDS_NEW_TAB_GUEST_SESSION_DESCRIPTION;
  int guest_tab_heading_ids = IDS_NEW_TAB_GUEST_SESSION_HEADING;
  int guest_tab_link_ids = IDS_LEARN_MORE;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  guest_tab_idr = IDR_GUEST_SESSION_TAB_HTML;

  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();

  if (connector->IsDeviceEnterpriseManaged()) {
    localized_strings.Set("enterpriseInfoVisible", "true");
    localized_strings.Set("enterpriseLearnMore",
                          l10n_util::GetStringUTF16(IDS_LEARN_MORE));
    localized_strings.Set("enterpriseInfoHintLink",
                          chrome::kLearnMoreEnterpriseURL);
    localized_strings.Set(
        "enterpriseLearnMoreA11yLabel",
        l10n_util::GetStringUTF16(
            IDS_NEW_TAB_ENTERPRISE_GUEST_SESSION_LEARN_MORE_ACCESSIBILITY_TEXT));
    std::u16string enterprise_info;
    if (connector->IsCloudManaged()) {
      const std::string enterprise_domain_manager =
          connector->GetEnterpriseDomainManager();
      enterprise_info = l10n_util::GetStringFUTF16(
          IDS_ASH_ENTERPRISE_DEVICE_MANAGED_BY, ui::GetChromeOSDeviceName(),
          base::UTF8ToUTF16(enterprise_domain_manager));
    } else {
      NOTREACHED_IN_MIGRATION() << "Unknown management type";
    }
    localized_strings.Set("enterpriseInfoMessage", enterprise_info);
  } else {
    localized_strings.Set("enterpriseInfoVisible", "false");
    localized_strings.Set("enterpriseInfoMessage", "");
    localized_strings.Set("enterpriseLearnMore", "");
    localized_strings.Set("enterpriseInfoHintLink", "");
    localized_strings.Set("enterpriseLearnMoreA11yLabel", "");
  }
#endif

  localized_strings.Set("guestTabDescription",
                        l10n_util::GetStringUTF16(guest_tab_description_ids));
  localized_strings.Set("guestTabHeading",
                        l10n_util::GetStringUTF16(guest_tab_heading_ids));
  localized_strings.Set("learnMore",
                        l10n_util::GetStringUTF16(guest_tab_link_ids));
  localized_strings.Set("learnMoreLink", guest_tab_link);
  localized_strings.Set(
      "learnMoreA11yLabel",
      l10n_util::GetStringUTF16(
          IDS_NEW_TAB_GUEST_SESSION_LEARN_MORE_ACCESSIBILITY_TEXT));

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &localized_strings);

  static const base::NoDestructor<scoped_refptr<base::RefCountedMemory>>
      guest_tab_html(
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              guest_tab_idr));
  CHECK(*guest_tab_html);
  ui::TemplateReplacements replacements;
  ui::TemplateReplacementsFromDictionaryValue(localized_strings, &replacements);
  new_tab_guest_html_ = base::MakeRefCounted<base::RefCountedString>(
      ReplaceTemplateExpressions(*guest_tab_html, replacements));
}

void NTPResourceCache::CreateNewTabIncognitoCSS(
    const content::WebContents::Getter& wc_getter) {
  auto* web_contents = wc_getter.Run();
  const ui::NativeTheme* native_theme =
      webui::GetNativeThemeDeprecated(web_contents);
  DCHECK(native_theme);

  const ui::ThemeProvider& tp = ThemeService::GetThemeProviderForProfile(
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  // Generate the replacements.
  ui::TemplateReplacements substitutions;

  // Cache-buster for background.
  substitutions["themeId"] =
      profile_->GetPrefs()->GetString(prefs::kCurrentThemeID);

  // Colors.
  const ui::ColorProvider& cp = web_contents->GetColorProvider();
  substitutions["colorBackground"] = color_utils::SkColorToRgbaString(
      GetThemeColor(native_theme, cp, kColorNewTabPageBackground));
  substitutions["backgroundPosition"] = GetNewTabBackgroundPositionCSS(tp);
  substitutions["backgroundTiling"] = GetNewTabBackgroundTilingCSS(tp);

  // Get our template.
  static const base::NoDestructor<scoped_refptr<base::RefCountedMemory>>
      new_tab_theme_css(
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              IDR_INCOGNITO_TAB_THEME_CSS));
  CHECK(*new_tab_theme_css);
  new_tab_incognito_css_ = base::MakeRefCounted<base::RefCountedString>(
      ReplaceTemplateExpressions(*new_tab_theme_css, substitutions));
}

void NTPResourceCache::CreateNewTabCSS(
    const content::WebContents::Getter& wc_getter) {
  auto* web_contents = wc_getter.Run();
  const ui::NativeTheme* native_theme =
      webui::GetNativeThemeDeprecated(web_contents);
  DCHECK(native_theme);

  const ui::ThemeProvider& tp =
      ThemeService::GetThemeProviderForProfile(profile_);
  const ui::ColorProvider& cp = web_contents->GetColorProvider();

  // Get our theme colors.
  SkColor color_background =
      GetThemeColor(native_theme, cp, kColorNewTabPageBackground);
  SkColor color_text = GetThemeColor(native_theme, cp, kColorNewTabPageText);
  SkColor color_text_light =
      GetThemeColor(native_theme, cp, kColorNewTabPageTextLight);

  SkColor color_section_border =
      GetThemeColor(native_theme, cp, kColorNewTabPageSectionBorder);

  // Generate the replacements.
  ui::TemplateReplacements substitutions;

  // Cache-buster for background.
  substitutions["themeId"] =
      profile_->GetPrefs()->GetString(prefs::kCurrentThemeID);

  // Colors.
  substitutions["colorBackground"] =
      color_utils::SkColorToRgbaString(color_background);
  substitutions["colorLink"] = color_utils::SkColorToRgbString(
      GetThemeColor(native_theme, cp, kColorNewTabPageLink));
  substitutions["backgroundPosition"] = GetNewTabBackgroundPositionCSS(tp);
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
  new_tab_css_ = base::MakeRefCounted<base::RefCountedString>(
      ReplaceTemplateExpressions(*new_tab_theme_css, substitutions));
}

void NTPResourceCache::OnPolicyChanged(const base::Value* previous,
                                       const base::Value* current) {
  new_tab_incognito_html_ = nullptr;
}
