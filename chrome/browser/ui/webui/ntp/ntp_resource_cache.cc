// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/webui/app_launcher_login_handler.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/browser/ui/webui/ntp/cookie_controls_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
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
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chromeos/strings/grit/chromeos_strings.h"
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

// The URL for the Learn More page shown on ephermal guest session new tab.
const char kLearnMoreEphemeralGuestSessionUrl[] =
    "https://support.google.com/chrome/?p=ui_guest";

std::string ReplaceTemplateExpressions(
    const scoped_refptr<base::RefCountedMemory>& bytes,
    const ui::TemplateReplacements& replacements) {
  return ui::ReplaceTemplateExpressions(
      base::StringPiece(reinterpret_cast<const char*>(bytes->front()),
                        bytes->size()),
      replacements);
}

}  // namespace

SkColor GetThemeColor(const ui::NativeTheme* native_theme,
                      const ui::ThemeProvider& tp,
                      int id) {
  SkColor color = tp.GetColor(id);
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
  profile_pref_change_registrar_.Add(prefs::kNtpShownPage, callback);
  profile_pref_change_registrar_.Add(prefs::kCookieControlsMode, callback);

  // TODO(crbug/1056916): Remove the global accessor to NativeTheme.
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

NTPResourceCache::WindowType NTPResourceCache::GetWindowType(
    Profile* profile, content::RenderProcessHost* render_host) {
  if (profile->IsGuestSession() || profile->IsEphemeralGuestProfile())
    return GUEST;

  // Sometimes the |profile| is the parent (non-incognito) version of the user
  // so we check the |render_host| if it is provided.
  if (render_host && render_host->GetBrowserContext()->IsOffTheRecord())
    profile = Profile::FromBrowserContext(render_host->GetBrowserContext());

  if (profile->IsIncognitoProfile())
    return INCOGNITO;
  if (profile->IsOffTheRecord())
    return NON_PRIMARY_OTR;

  return NORMAL;
}

base::RefCountedMemory* NTPResourceCache::GetNewTabGuestHTML() {
  // TODO(crbug.com/1134111): For full launch of ephemeral Guest profiles,
  // instead of the below code block, use IdentityManager for ephemeral Guest
  // profiles to check sign in status and return either
  // |CreateNewTabEphemeralGuestSignedInHTML()| or
  // |CreateNewTabEphemeralGuestSignedOutHTML()|.
  if (!new_tab_guest_html_) {
    GuestNTPInfo guest_ntp_info{kLearnMoreGuestSessionUrl, IDR_GUEST_TAB_HTML,
                                IDS_NEW_TAB_GUEST_SESSION_HEADING,
                                IDS_NEW_TAB_GUEST_SESSION_DESCRIPTION};
    new_tab_guest_html_ = CreateNewTabGuestHTML(guest_ntp_info);
  }

  return new_tab_guest_html_.get();
}

base::RefCountedMemory* NTPResourceCache::GetNewTabHTML(WindowType win_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  switch (win_type) {
    case GUEST:
      return GetNewTabGuestHTML();

    case INCOGNITO:
      if (!new_tab_incognito_html_)
        CreateNewTabIncognitoHTML();
      return new_tab_incognito_html_.get();

    case NON_PRIMARY_OTR:
      if (!new_tab_non_primary_otr_html_) {
        std::string empty_html;
        new_tab_non_primary_otr_html_ =
            base::RefCountedString::TakeString(&empty_html);
      }
      return new_tab_non_primary_otr_html_.get();

    case NORMAL:
      NOTREACHED();
      return nullptr;
  }
}

base::RefCountedMemory* NTPResourceCache::GetNewTabCSS(
    WindowType win_type,
    content::WebContents::Getter wc_getter) {
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
  // TODO(crbug/1056916): Remove the global accessor to NativeTheme.
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
  new_tab_guest_signed_in_html_ = nullptr;
}

void NTPResourceCache::CreateNewTabIncognitoHTML() {
  ui::TemplateReplacements replacements;

  // Ensure passing off-the-record profile; |profile_| is not an OTR profile.
  DCHECK(!profile_->IsOffTheRecord());
  DCHECK(profile_->HasAnyOffTheRecordProfile());

  // Cookie controls service returns the same result for all off-the-record
  // profiles, so it doesn't matter which of them we use.
  Profile* incognito_profile = profile_->GetAllOffTheRecordProfiles()[0];
  CookieControlsService* cookie_controls_service =
      CookieControlsServiceFactory::GetForProfile(incognito_profile);

  replacements["incognitoTabDescription"] =
      l10n_util::GetStringUTF8(reading_list::switches::IsReadingListEnabled()
                                   ? IDS_NEW_TAB_OTR_SUBTITLE_WITH_READING_LIST
                                   : IDS_NEW_TAB_OTR_SUBTITLE);
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

  // The ThemeProvider can have different behavior depending on regular or
  // Incognito profile. Therefore, making use of Incognito profile explicitly
  // here.
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

  std::string full_html =
      ReplaceTemplateExpressions(*incognito_tab_html, replacements);

  new_tab_incognito_html_ = base::RefCountedString::TakeString(&full_html);
}

base::RefCountedMemory*
NTPResourceCache::CreateNewTabEphemeralGuestSignedInHTML() {
  if (!new_tab_guest_signed_in_html_) {
    GuestNTPInfo guest_ntp_info{
        kLearnMoreEphemeralGuestSessionUrl,
        IDR_EPHEMERAL_GUEST_TAB_HTML,
        IDS_NEW_TAB_EPHEMERAL_GUEST_SESSION_HEADING_SIGNED_IN,
        IDS_NEW_TAB_EPHEMERAL_GUEST_SESSION_DESCRIPTION_SIGNED_IN,
        IDS_NEW_TAB_EPHEMERAL_GUEST_NOT_SAVED_SIGNED_IN,
        IDS_NEW_TAB_EPHEMERAL_GUEST_SAVED};
    new_tab_guest_signed_in_html_ = CreateNewTabGuestHTML(guest_ntp_info);
  }

  return new_tab_guest_signed_in_html_.get();
}

base::RefCountedMemory*
NTPResourceCache::CreateNewTabEphemeralGuestSignedOutHTML() {
  // Clear cached signed in HTML on sign out to avoid loading previously cached
  // user name from other signed in guest sessions.
  new_tab_guest_signed_in_html_ = nullptr;

  if (!new_tab_guest_signed_out_html_) {
    GuestNTPInfo guest_ntp_info{
        kLearnMoreEphemeralGuestSessionUrl,
        IDR_EPHEMERAL_GUEST_TAB_HTML,
        IDS_NEW_TAB_EPHEMERAL_GUEST_SESSION_HEADING_SIGNED_OUT,
        IDS_NEW_TAB_EPHEMERAL_GUEST_SESSION_DESCRIPTION_SIGNED_OUT,
        IDS_NEW_TAB_EPHEMERAL_GUEST_NOT_SAVED_SIGNED_OUT,
        IDS_NEW_TAB_EPHEMERAL_GUEST_SAVED};
    new_tab_guest_signed_out_html_ = CreateNewTabGuestHTML(guest_ntp_info);
  }
  return new_tab_guest_signed_out_html_.get();
}

scoped_refptr<base::RefCountedString> NTPResourceCache::CreateNewTabGuestHTML(
    const GuestNTPInfo& guest_ntp_info) {
  base::DictionaryValue localized_strings;
  localized_strings.SetString("title",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
  int guest_tab_idr = guest_ntp_info.html_idr;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  guest_tab_idr = IDR_GUEST_SESSION_TAB_HTML;

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();

  if (connector->IsEnterpriseManaged()) {
    localized_strings.SetString("enterpriseInfoVisible", "true");
    localized_strings.SetString("enterpriseLearnMore",
                                l10n_util::GetStringUTF16(IDS_LEARN_MORE));
    localized_strings.SetString("enterpriseInfoHintLink",
                                chrome::kLearnMoreEnterpriseURL);
    std::u16string enterprise_info;
    if (connector->IsCloudManaged()) {
      const std::string enterprise_domain_manager =
          connector->GetEnterpriseDomainManager();
      enterprise_info = l10n_util::GetStringFUTF16(
          IDS_ASH_ENTERPRISE_DEVICE_MANAGED_BY, ui::GetChromeOSDeviceName(),
          base::UTF8ToUTF16(enterprise_domain_manager));
    } else if (connector->IsActiveDirectoryManaged()) {
      enterprise_info = l10n_util::GetStringFUTF16(
          IDS_ASH_ENTERPRISE_DEVICE_MANAGED, ui::GetChromeOSDeviceName());
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

  if (guest_ntp_info.features_ids != -1) {
    localized_strings.SetString(
        "guestTabFeatures",
        l10n_util::GetStringUTF16(guest_ntp_info.features_ids));
  }

  if (guest_ntp_info.warnings_ids != -1) {
    localized_strings.SetString(
        "guestTabWarning",
        l10n_util::GetStringUTF16(guest_ntp_info.warnings_ids));
  }

  localized_strings.SetString(
      "guestTabDescription",
      l10n_util::GetStringUTF16(guest_ntp_info.description_ids));
  // TODO(crbug.com/1134111): Replace placeholder with user's name in the
  // greeting message when the guest sign in functionality is implemented.
  localized_strings.SetString(
      "guestTabHeading", l10n_util::GetStringUTF16(guest_ntp_info.heading_ids));
  localized_strings.SetString("learnMore",
                              l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  localized_strings.SetString("learnMoreLink", guest_ntp_info.learn_more_link);

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

  return base::RefCountedString::TakeString(&full_html);
}

void NTPResourceCache::CreateNewTabIncognitoCSS(
    const content::WebContents::Getter wc_getter) {
  const ui::NativeTheme* native_theme = webui::GetNativeTheme(wc_getter.Run());
  const ui::ThemeProvider& tp = ThemeService::GetThemeProviderForProfile(
      profile_->GetPrimaryOTRProfile());

  // Generate the replacements.
  ui::TemplateReplacements substitutions;

  // Cache-buster for background.
  substitutions["themeId"] =
      profile_->GetPrefs()->GetString(prefs::kCurrentThemeID);

  // Colors.
  substitutions["colorBackground"] = color_utils::SkColorToRgbaString(
      GetThemeColor(native_theme, tp, ThemeProperties::COLOR_NTP_BACKGROUND));
  substitutions["backgroundPosition"] = GetNewTabBackgroundPositionCSS(tp);
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

void NTPResourceCache::CreateNewTabCSS(
    const content::WebContents::Getter wc_getter) {
  const ui::NativeTheme* native_theme = webui::GetNativeTheme(wc_getter.Run());
  const ui::ThemeProvider& tp =
      ThemeService::GetThemeProviderForProfile(profile_);

  // Get our theme colors.
  SkColor color_background =
      GetThemeColor(native_theme, tp, ThemeProperties::COLOR_NTP_BACKGROUND);
  SkColor color_text =
      GetThemeColor(native_theme, tp, ThemeProperties::COLOR_NTP_TEXT);
  SkColor color_text_light =
      GetThemeColor(native_theme, tp, ThemeProperties::COLOR_NTP_TEXT_LIGHT);

  SkColor color_header =
      GetThemeColor(native_theme, tp, ThemeProperties::COLOR_NTP_HEADER);
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
      GetThemeColor(native_theme, tp, ThemeProperties::COLOR_NTP_LINK));
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

  // Create the string from our template and the replacements.
  std::string css_string =
      ReplaceTemplateExpressions(*new_tab_theme_css, substitutions);
  new_tab_css_ = base::RefCountedString::TakeString(&css_string);
}

void NTPResourceCache::OnPolicyChanged(const base::Value* previous,
                                       const base::Value* current) {
  new_tab_incognito_html_ = nullptr;
}
