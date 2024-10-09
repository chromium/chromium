// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/location_bar_model_impl.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/omnibox/browser/location_bar_model_delegate.h"
#include "components/omnibox/browser/location_bar_model_util.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/origin.h"

#if (!BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !BUILDFLAG(IS_IOS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif

using metrics::OmniboxEventProto;

LocationBarModelImpl::LocationBarModelImpl(LocationBarModelDelegate* delegate,
                                           size_t max_url_display_chars)
    : delegate_(delegate), max_url_display_chars_(max_url_display_chars) {
  DCHECK(delegate_);
}

LocationBarModelImpl::~LocationBarModelImpl() = default;

// LocationBarModelImpl Implementation.
std::u16string LocationBarModelImpl::GetFormattedFullURL() const {
  return GetFormattedURL(url_formatter::kFormatUrlOmitDefaults);
}

std::u16string LocationBarModelImpl::GetURLForDisplay() const {
  url_formatter::FormatUrlTypes format_types =
      url_formatter::kFormatUrlOmitDefaults;
  if (delegate_->ShouldTrimDisplayUrlAfterHostName()) {
    format_types |= url_formatter::kFormatUrlTrimAfterHost;
  }

#if BUILDFLAG(IS_IOS)
  format_types |= url_formatter::kFormatUrlTrimAfterHost;
#endif

  format_types |= url_formatter::kFormatUrlOmitHTTPS;
  format_types |= url_formatter::kFormatUrlOmitTrivialSubdomains;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // On desktop, the File chip makes the scheme redundant in the steady state.
  format_types |= url_formatter::kFormatUrlOmitFileScheme;
#endif

  if (dom_distiller::url_utils::IsDistilledPage(GetURL())) {
    // We explicitly elide the scheme here to ensure that HTTPS and HTTP will
    // be removed for display: Reader mode pages should not display a scheme,
    // and should only run on HTTP/HTTPS pages.
    // Users will be able to see the scheme when the URL is focused or being
    // edited in the omnibox.
    format_types |= url_formatter::kFormatUrlOmitHTTP;
    format_types |= url_formatter::kFormatUrlOmitHTTPS;
  }

  return GetFormattedURL(format_types);
}

std::u16string LocationBarModelImpl::GetFormattedURL(
    url_formatter::FormatUrlTypes format_types) const {
  if (!ShouldDisplayURL())
    return std::u16string{};

  // Reset |format_types| to prevent elision of URLs when relevant extension or
  // pref is enabled.
  if (delegate_->ShouldPreventElision()) {
    format_types = url_formatter::kFormatUrlOmitDefaults &
                   ~url_formatter::kFormatUrlOmitHTTP;
  }

  GURL url(GetURL());

#if BUILDFLAG(IS_IOS)
  // On iOS, the blob: display URLs should be simply the domain name. However,
  // url_formatter parses everything past blob: as path, not domain, so swap
  // the url here to be just origin.
  if (url.SchemeIsBlob()) {
    url = url::Origin::Create(url).GetURL();
  }
#endif  // BUILDFLAG(IS_IOS)

  // Special handling for dom-distiller:. Instead of showing internal reader
  // mode URLs, show the original article URL in the omnibox.
  // Note that this does not disallow the user from seeing the distilled page
  // URL in the view-source url or devtools. Note that this also impacts
  // GetFormattedFullURL which uses GetFormattedURL as a helper.
  // Virtual URLs were not a good solution for Reader Mode URLs because some
  // security UI is based off of the virtual URL rather than the original URL,
  // and Reader Mode has its own security chip. In addition virtual URLs would
  // add a lot of complexity around passing necessary URL parameters to the
  // Reader Mode pages.
  // Note: if the URL begins with dom-distiller:// but is invalid we display it
  // as-is because it cannot be transformed into an article URL.
  if (dom_distiller::url_utils::IsDistilledPage(url))
    url = dom_distiller::url_utils::GetOriginalUrlFromDistillerUrl(url);

  // Note that we can't unescape spaces here, because if the user copies this
  // and pastes it into another program, that program may think the URL ends at
  // the space.
  const std::u16string formatted_text =
      delegate_->FormattedStringWithEquivalentMeaning(
          url, url_formatter::FormatUrl(url, format_types,
                                        base::UnescapeRule::NORMAL, nullptr,
                                        nullptr, nullptr));

  // Truncating the URL breaks editing and then pressing enter, but hopefully
  // people won't try to do much with such enormous URLs anyway. If this becomes
  // a real problem, we could perhaps try to keep some sort of different "elided
  // visible URL" where editing affects and reloads the "real underlying URL",
  // but this seems very tricky for little gain.
  return gfx::TruncateString(formatted_text, max_url_display_chars_,
                             gfx::CHARACTER_BREAK);
}

GURL LocationBarModelImpl::GetURL() const {
  GURL url;
  return (ShouldDisplayURL() && delegate_->GetURL(&url))
             ? url
             : GURL(url::kAboutBlankURL);
}

security_state::SecurityLevel LocationBarModelImpl::GetSecurityLevel() const {
  // When empty, assume no security style.
  if (!ShouldDisplayURL())
    return security_state::NONE;

  return delegate_->GetSecurityLevel();
}

net::CertStatus LocationBarModelImpl::GetCertStatus() const {
  // When empty, assume no cert status.
  if (!ShouldDisplayURL())
    return 0;

  return delegate_->GetCertStatus();
}

OmniboxEventProto::PageClassification
LocationBarModelImpl::GetPageClassification(bool is_prefetch) const {
  // We may be unable to fetch the current URL during startup or shutdown when
  // the omnibox exists but there is no attached page.
  GURL gurl;
  if (!delegate_->GetURL(&gurl)) {
    return OmniboxEventProto::OTHER;
  }
  if (delegate_->IsNewTabPage()) {
    return is_prefetch ? OmniboxEventProto::NTP_ZPS_PREFETCH
               : OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS;
  }
  if (!gurl.is_valid()) {
    return OmniboxEventProto::INVALID_SPEC;
  }
  if (delegate_->IsNewTabPageURL(gurl)) {
    return is_prefetch ? OmniboxEventProto::NTP_ZPS_PREFETCH
                       : OmniboxEventProto::NTP;
  }
  if (gurl.spec() == url::kAboutBlankURL) {
    return OmniboxEventProto::BLANK;
  }
  if (delegate_->IsHomePage(gurl)) {
    return OmniboxEventProto::HOME_PAGE;
  }

  TemplateURLService* template_url_service = delegate_->GetTemplateURLService();
  if (template_url_service &&
      template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
          gurl)) {
    return is_prefetch ? OmniboxEventProto::SRP_ZPS_PREFETCH
                       : OmniboxEventProto::
                             SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT;
  }

  return is_prefetch ? OmniboxEventProto::OTHER_ZPS_PREFETCH
                     : OmniboxEventProto::OTHER;
}

const gfx::VectorIcon& LocationBarModelImpl::GetVectorIcon() const {
#if (!BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !BUILDFLAG(IS_IOS)
  auto* const icon_override = delegate_->GetVectorIconOverride();
  if (icon_override)
    return *icon_override;

  if (IsOfflinePage())
    return omnibox::kOfflinePinIcon;
#endif

  return location_bar_model::GetSecurityVectorIcon(
      GetSecurityLevel(),
      delegate_->ShouldUseUpdatedConnectionSecurityIndicators(),
      delegate_->GetVisibleSecurityState()->malicious_content_status);
}

std::u16string LocationBarModelImpl::GetSecureDisplayText() const {
  // Note that display text will be implicitly used as the accessibility text.
  // GetSecureAccessibilityText() handles special cases when no display text is
  // set.

  if (IsOfflinePage())
    return l10n_util::GetStringUTF16(IDS_OFFLINE_VERBOSE_STATE);

  switch (GetSecurityLevel()) {
    case security_state::WARNING:
      return l10n_util::GetStringUTF16(IDS_NOT_SECURE_VERBOSE_STATE);
    case security_state::SECURE:
      return std::u16string();
    case security_state::DANGEROUS: {
      std::unique_ptr<security_state::VisibleSecurityState>
          visible_security_state = delegate_->GetVisibleSecurityState();

      // Don't show any text in the security indicator for sites on the billing
      // interstitial list or blocked by the enterprise administrator.
      if (visible_security_state->malicious_content_status ==
              security_state::MALICIOUS_CONTENT_STATUS_BILLING ||
          visible_security_state->malicious_content_status ==
              security_state::MALICIOUS_CONTENT_STATUS_MANAGED_POLICY_BLOCK ||
          visible_security_state->malicious_content_status ==
              security_state::MALICIOUS_CONTENT_STATUS_MANAGED_POLICY_WARN) {
        return std::u16string();
      }

      bool fails_malware_check =
          visible_security_state->malicious_content_status !=
          security_state::MALICIOUS_CONTENT_STATUS_NONE;
      return l10n_util::GetStringUTF16(fails_malware_check
                                           ? IDS_DANGEROUS_VERBOSE_STATE
                                           : IDS_NOT_SECURE_VERBOSE_STATE);
    }
    default:
      return std::u16string();
  }
}

std::u16string LocationBarModelImpl::GetSecureAccessibilityText() const {
  auto display_text = GetSecureDisplayText();
  if (!display_text.empty())
    return display_text;

  switch (GetSecurityLevel()) {
    case security_state::SECURE:
      return l10n_util::GetStringUTF16(IDS_SECURE_VERBOSE_STATE);
    default:
      return std::u16string();
  }
}

bool LocationBarModelImpl::ShouldDisplayURL() const {
  return delegate_->ShouldDisplayURL();
}

bool LocationBarModelImpl::IsOfflinePage() const {
  return delegate_->IsOfflinePage();
}

bool LocationBarModelImpl::ShouldPreventElision() const {
  return delegate_->ShouldPreventElision();
}

bool LocationBarModelImpl::ShouldUseUpdatedConnectionSecurityIndicators()
    const {
  return delegate_->ShouldUseUpdatedConnectionSecurityIndicators();
}
