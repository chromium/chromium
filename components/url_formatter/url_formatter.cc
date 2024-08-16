// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/url_formatter/url_formatter.h"

#include <algorithm>
#include <ostream>
#include <string_view>
#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_local_storage.h"
#include "build/build_config.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/icu/source/common/unicode/uidna.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace url_formatter {

namespace {

const char kWww[] = "www.";
constexpr size_t kWwwLength = 4;
const char kMobilePrefix[] = "m.";
constexpr size_t kMobilePrefixLength = 2;

IDNConversionResult IDNToUnicodeWithAdjustments(
    std::string_view host,
    base::OffsetAdjuster::Adjustments* adjustments);

// Result of converting a single IDN component (i.e. label) to unicode.
struct ComponentResult {
  // Set to true if the component is converted to unicode.
  bool converted = false;
  // Set to true if the component is IDN, even if it's not converted to unicode.
  bool has_idn_component = false;
  // Result of the IDN spoof check.
  IDNSpoofChecker::Result spoof_check_result = IDNSpoofChecker::Result::kNone;
};

ComponentResult IDNToUnicodeOneComponent(
    std::u16string_view comp,
    std::string_view top_level_domain,
    std::u16string_view top_level_domain_unicode,
    bool ignore_spoof_check_results,
    std::u16string* out);

class AppendComponentTransform {
 public:
  AppendComponentTransform() = default;
  virtual ~AppendComponentTransform() = default;

  virtual std::u16string Execute(
      const std::string& component_text,
      base::OffsetAdjuster::Adjustments* adjustments) const = 0;

  // NOTE: No DISALLOW_COPY_AND_ASSIGN here, since gcc < 4.3.0 requires an
  // accessible copy constructor in order to call AppendFormattedComponent()
  // with an inline temporary (see http://gcc.gnu.org/bugs/#cxx%5Frvalbind ).
};

class HostComponentTransform : public AppendComponentTransform {
 public:
  HostComponentTransform(bool trim_trivial_subdomains, bool trim_mobile_prefix)
      : trim_trivial_subdomains_(trim_trivial_subdomains),
        trim_mobile_prefix_(trim_mobile_prefix) {}

 private:
  std::u16string Execute(
      const std::string& component_text,
      base::OffsetAdjuster::Adjustments* adjustments) const override {
    // Nothing to change.
    if (!trim_trivial_subdomains_ && !trim_mobile_prefix_)
      return IDNToUnicodeWithAdjustments(component_text, adjustments).result;

    std::string stripped_component_text = component_text;
    if (base::StartsWith(component_text, "www.m.") &&
        trim_trivial_subdomains_ && trim_mobile_prefix_) {
      stripped_component_text = StripWWW(stripped_component_text);
      stripped_component_text = StripMobilePrefix(stripped_component_text);
    } else if (base::StartsWith(component_text, "m.www.") &&
               trim_mobile_prefix_ && trim_trivial_subdomains_) {
      stripped_component_text = StripMobilePrefix(stripped_component_text);
      stripped_component_text = StripWWW(stripped_component_text);
    } else {
      if (trim_trivial_subdomains_) {
        stripped_component_text = StripWWW(component_text);
      }
      if (trim_mobile_prefix_) {
        stripped_component_text = StripMobilePrefix(stripped_component_text);
      }
    }

    // If StripWWW() and StripMobilePrefix() did nothing, then "www." and "m."
    // weren't a prefix, or it otherwise didn't meet conditions for stripping
    // "www." (such as intranet hostnames). In this case, no adjustments for
    // trivial subdomains are needed.
    if (stripped_component_text == component_text)
      return IDNToUnicodeWithAdjustments(component_text, adjustments).result;

    base::OffsetAdjuster::Adjustments offset_adjustments;

    if (component_text.length() ==
        stripped_component_text.length() + kMobilePrefixLength + kWwwLength) {
      // Add www. and m. offsets.
      offset_adjustments.push_back(
          base::OffsetAdjuster::Adjustment(0, kWwwLength, 0));
      offset_adjustments.push_back(
          base::OffsetAdjuster::Adjustment(0, kMobilePrefixLength, 0));
    } else if (component_text.length() ==
               stripped_component_text.length() + kWwwLength) {
      // Add www. offset.
      offset_adjustments.push_back(
          base::OffsetAdjuster::Adjustment(0, kWwwLength, 0));
    } else if (component_text.length() ==
               stripped_component_text.length() + kMobilePrefixLength) {
      // Add m. offset
      offset_adjustments.push_back(
          base::OffsetAdjuster::Adjustment(0, kMobilePrefixLength, 0));
    }
    std::u16string unicode_result =
        IDNToUnicodeWithAdjustments(stripped_component_text, adjustments)
            .result;
    base::OffsetAdjuster::MergeSequentialAdjustments(offset_adjustments,
                                                     adjustments);
    return unicode_result;
  }

  bool trim_trivial_subdomains_;
  bool trim_mobile_prefix_;
};

class NonHostComponentTransform : public AppendComponentTransform {
 public:
  explicit NonHostComponentTransform(base::UnescapeRule::Type unescape_rules)
      : unescape_rules_(unescape_rules) {}

 private:
  std::u16string Execute(
      const std::string& component_text,
      base::OffsetAdjuster::Adjustments* adjustments) const override {
    return (unescape_rules_ == base::UnescapeRule::NONE)
               ? base::UTF8ToUTF16WithAdjustments(component_text, adjustments)
               : base::UnescapeAndDecodeUTF8URLComponentWithAdjustments(
                     component_text, unescape_rules_, adjustments);
  }

  const base::UnescapeRule::Type unescape_rules_;
};

// Transforms the portion of |spec| covered by |original_component| according to
// |transform|.  Appends the result to |output|.  If |output_component| is
// non-NULL, its start and length are set to the transformed component's new
// start and length.  If |adjustments| is non-NULL, appends adjustments (if
// any) that reflect the transformation the original component underwent to
// become the transformed value appended to |output|.
void AppendFormattedComponent(const std::string& spec,
                              const url::Component& original_component,
                              const AppendComponentTransform& transform,
                              std::u16string* output,
                              url::Component* output_component,
                              base::OffsetAdjuster::Adjustments* adjustments) {
  DCHECK(output);
  if (original_component.is_nonempty()) {
    size_t original_component_begin =
        static_cast<size_t>(original_component.begin);
    size_t output_component_begin = output->length();
    std::string component_str(spec, original_component_begin,
                              static_cast<size_t>(original_component.len));

    // Transform |component_str| and modify |adjustments| appropriately.
    base::OffsetAdjuster::Adjustments component_transform_adjustments;
    output->append(
        transform.Execute(component_str, &component_transform_adjustments));

    // Shift all the adjustments made for this component so the offsets are
    // valid for the original string and add them to |adjustments|.
    for (auto& component_transform_adjustment :
         component_transform_adjustments) {
      component_transform_adjustment.original_offset +=
          original_component_begin;
    }
    if (adjustments) {
      adjustments->insert(adjustments->end(),
                          component_transform_adjustments.begin(),
                          component_transform_adjustments.end());
    }

    // Set positions of the parsed component.
    if (output_component) {
      output_component->begin = static_cast<int>(output_component_begin);
      output_component->len =
          static_cast<int>(output->length() - output_component_begin);
    }
  } else if (output_component) {
    output_component->reset();
  }
}

// If |component| is valid, its begin is incremented by |delta|.
void AdjustComponent(int delta, url::Component* component) {
  if (!component->is_valid())
    return;

  DCHECK(delta >= 0 || component->begin >= -delta);
  component->begin += delta;
}

// Adjusts all the components of |parsed| by |delta|, except for the scheme.
void AdjustAllComponentsButScheme(int delta, url::Parsed* parsed) {
  AdjustComponent(delta, &(parsed->username));
  AdjustComponent(delta, &(parsed->password));
  AdjustComponent(delta, &(parsed->host));
  AdjustComponent(delta, &(parsed->port));
  AdjustComponent(delta, &(parsed->path));
  AdjustComponent(delta, &(parsed->query));
  AdjustComponent(delta, &(parsed->ref));
}

// Helper for FormatUrlWithOffsets().
std::u16string FormatViewSourceUrl(
    const GURL& url,
    FormatUrlTypes format_types,
    base::UnescapeRule::Type unescape_rules,
    url::Parsed* new_parsed,
    size_t* prefix_end,
    base::OffsetAdjuster::Adjustments* adjustments) {
  DCHECK(new_parsed);
  static constexpr std::u16string_view kViewSource = u"view-source:";

  // The URL embedded within view-source should never have destructive elisions
  // applied to it. Users of view-source likely want to see the full URL.
  format_types &= ~kFormatUrlOmitHTTPS;
  format_types &= ~kFormatUrlOmitTrivialSubdomains;
  format_types &= ~kFormatUrlTrimAfterHost;
  format_types &= ~kFormatUrlOmitFileScheme;

  // Format the underlying URL and record adjustments.
  const std::string& url_str(url.possibly_invalid_spec());
  adjustments->clear();
  std::u16string result = base::StrCat(
      {kViewSource, FormatUrlWithAdjustments(
                        GURL(url_str.substr(kViewSource.size())), format_types,
                        unescape_rules, new_parsed, prefix_end, adjustments)});
  // Revise |adjustments| by shifting to the offsets to prefix that the above
  // call to FormatUrl didn't get to see.
  for (auto& adjustment : *adjustments)
    adjustment.original_offset += kViewSource.size();

  // Adjust positions of the parsed components.
  if (new_parsed->scheme.is_nonempty()) {
    // Assume "view-source:real-scheme" as a scheme.
    new_parsed->scheme.len += kViewSource.size();
  } else {
    new_parsed->scheme.begin = 0;
    new_parsed->scheme.len = kViewSource.size() - 1;
  }
  AdjustAllComponentsButScheme(kViewSource.size(), new_parsed);

  if (prefix_end)
    *prefix_end += kViewSource.size();

  return result;
}

base::LazyInstance<IDNSpoofChecker>::Leaky g_idn_spoof_checker =
    LAZY_INSTANCE_INITIALIZER;

// Computes the top level domain from |host|. top_level_domain_unicode will
// contain the unicode version of top_level_domain. top_level_domain_unicode can
// remain empty if the TLD is not well formed punycode.
void GetTopLevelDomain(std::string_view host,
                       std::string_view* top_level_domain,
                       std::u16string* top_level_domain_unicode) {
  size_t last_dot = host.rfind('.');
  if (last_dot == std::string_view::npos) {
    return;
  }

  *top_level_domain = host.substr(last_dot + 1);
  std::u16string tld16;
  tld16.reserve(top_level_domain->length());
  tld16.insert(tld16.end(), top_level_domain->begin(), top_level_domain->end());

  // Convert the TLD to unicode, ignoring the spoof check results. This will
  // always decode the input to unicode as long as it's valid punycode.
  IDNToUnicodeOneComponent(tld16, std::string(), std::u16string(),
                           /*ignore_spoof_check_results=*/true,
                           top_level_domain_unicode);
}

IDNConversionResult IDNToUnicodeWithAdjustmentsImpl(
    std::string_view host,
    base::OffsetAdjuster::Adjustments* adjustments,
    bool ignore_spoof_check_results) {
  if (adjustments)
    adjustments->clear();
  // Convert the ASCII input to a std::u16string for ICU.
  std::u16string host16;
  host16.reserve(host.length());
  host16.insert(host16.end(), host.begin(), host.end());

  // Compute the top level domain to be used in spoof checks later.
  std::string_view top_level_domain;
  std::u16string top_level_domain_unicode;
  GetTopLevelDomain(host, &top_level_domain, &top_level_domain_unicode);

  IDNConversionResult result;
  // Do each component of the host separately, since we enforce script matching
  // on a per-component basis.
  std::u16string out16;
  for (size_t component_start = 0, component_end;
       component_start < host16.length(); component_start = component_end + 1) {
    // Find the end of the component.
    component_end = host16.find('.', component_start);
    if (component_end == std::u16string::npos)
      component_end = host16.length();  // For getting the last component.
    size_t component_length = component_end - component_start;
    size_t new_component_start = out16.length();
    ComponentResult component_result;
    if (component_end > component_start) {
      // Add the substring that we just found.
      component_result = IDNToUnicodeOneComponent(
          {host16.data() + component_start, component_length}, top_level_domain,
          top_level_domain_unicode, ignore_spoof_check_results, &out16);
      result.has_idn_component |= component_result.has_idn_component;
      if (component_result.spoof_check_result !=
              IDNSpoofChecker::Result::kNone &&
          (result.spoof_check_result == IDNSpoofChecker::Result::kNone ||
           result.spoof_check_result == IDNSpoofChecker::Result::kSafe)) {
        result.spoof_check_result = component_result.spoof_check_result;
      }
    }
    size_t new_component_length = out16.length() - new_component_start;

    if (component_result.converted && adjustments) {
      adjustments->push_back(base::OffsetAdjuster::Adjustment(
          component_start, component_length, new_component_length));
    }

    // Need to add the dot we just found (if we found one).
    if (component_end < host16.length())
      out16.push_back('.');
  }

  result.result = out16;

  // Leave as punycode any inputs that spoof top domains.
  if (result.has_idn_component) {
    result.matching_top_domain =
        g_idn_spoof_checker.Get().GetSimilarTopDomain(out16);
    if (!ignore_spoof_check_results &&
        !result.matching_top_domain.domain.empty()) {
      if (adjustments)
        adjustments->clear();
      result.result = host16;
    }
  }

  return result;
}

// TODO(brettw): We may want to skip this step in the case of file URLs to
// allow unicode UNC hostnames regardless of encodings.
IDNConversionResult IDNToUnicodeWithAdjustments(
    std::string_view host,
    base::OffsetAdjuster::Adjustments* adjustments) {
  return IDNToUnicodeWithAdjustmentsImpl(host, adjustments,
                                         /*ignore_spoof_check_results=*/false);
}

IDNConversionResult UnsafeIDNToUnicodeWithAdjustments(
    std::string_view host,
    base::OffsetAdjuster::Adjustments* adjustments) {
  return IDNToUnicodeWithAdjustmentsImpl(host, adjustments,
                                         /*ignore_spoof_check_results=*/true);
}

// Returns true if the given Unicode host component is safe to display to the
// user. Note that this function does not deal with pure ASCII domain labels at
// all even though it's possible to make up look-alike labels with ASCII
// characters alone.
IDNSpoofChecker::Result SpoofCheckIDNComponent(
    std::u16string_view label,
    std::string_view top_level_domain,
    std::u16string_view top_level_domain_unicode) {
  return g_idn_spoof_checker.Get().SafeToDisplayAsUnicode(
      label, top_level_domain, top_level_domain_unicode);
}

// A wrapper to use LazyInstance<>::Leaky with ICU's UIDNA, a C pointer to
// a UTS46/IDNA 2008 handling object opened with uidna_openUTS46().
//
// We use UTS46 with BiDiCheck to migrate from IDNA 2003 to IDNA 2008 with the
// backward compatibility in mind. What it does:
//
// 1. Use the up-to-date Unicode data.
// 2. Define a case folding/mapping with the up-to-date Unicode data as in
//    IDNA 2003.
// 3. Use transitional mechanism for 4 deviation characters (sharp-s,
//    final sigma, ZWJ and ZWNJ) for now.
// 4. Continue to allow symbols and punctuations.
// 5. Apply new BiDi check rules more permissive than the IDNA 2003 BiDI rules.
// 6. Do not apply STD3 rules
// 7. Do not allow unassigned code points.
//
// It also closely matches what IE 10 does except for the BiDi check (
// http://goo.gl/3XBhqw ).
// See http://http://unicode.org/reports/tr46/ and references therein/ for more
// details.
struct UIDNAWrapper {
  UIDNAWrapper() {
    UErrorCode err = U_ZERO_ERROR;
    // TODO(jungshik): Change options as different parties (browsers,
    // registrars, search engines) converge toward a consensus.
    value = uidna_openUTS46(UIDNA_CHECK_BIDI, &err);
    CHECK(U_SUCCESS(err)) << "failed to open UTS46 data with error: "
                          << u_errorName(err)
                          << ". If you see this error message in a test "
                          << "environment your test environment likely lacks "
                          << "the required data tables for libicu. See "
                          << "https://crbug.com/778929.";
  }

  raw_ptr<UIDNA> value;
};

base::LazyInstance<UIDNAWrapper>::Leaky g_uidna = LAZY_INSTANCE_INITIALIZER;

// Converts one component (label) of a host (between dots) to Unicode if safe.
// If |ignore_spoof_check_results| is true and input is valid unicode, ignores
// spoof check results and always converts the input to unicode. The result will
// be APPENDED to the given output string and will be the same as the input if
// it is not IDN in ACE/punycode or the IDN is unsafe to display. Returns true
// if conversion was made. Sets |has_idn_component| to true if the input has
// IDN, regardless of whether it was converted to unicode or not.
ComponentResult IDNToUnicodeOneComponent(
    std::u16string_view comp,
    std::string_view top_level_domain,
    std::u16string_view top_level_domain_unicode,
    bool ignore_spoof_check_results,
    std::u16string* out) {
  DCHECK(out);
  ComponentResult result;
  if (comp.empty())
    return result;

  // Early return if the input cannot be an IDN component.
  // Valid punycode must not end with a dash.
  static constexpr char16_t kIdnPrefix[] = u"xn--";
  if (!base::StartsWith(comp, kIdnPrefix) || comp.back() == '-') {
    out->append(comp);
    return result;
  }

  UIDNA* uidna = g_uidna.Get().value;
  DCHECK(uidna != nullptr);
  size_t original_length = out->length();
  int32_t output_length = 64;
  UIDNAInfo info = UIDNA_INFO_INITIALIZER;
  UErrorCode status;
  do {
    out->resize(original_length + output_length);
    status = U_ZERO_ERROR;
    // This returns the actual length required. If this is more than 64
    // code units, |status| will be U_BUFFER_OVERFLOW_ERROR and we'll try
    // the conversion again, but with a sufficiently large buffer.
    output_length = uidna_labelToUnicode(
        uidna, comp.data(), static_cast<int32_t>(comp.size()),
        &(*out)[original_length], output_length, &info, &status);
  } while ((status == U_BUFFER_OVERFLOW_ERROR && info.errors == 0));

  if (U_SUCCESS(status) && info.errors == 0) {
    result.has_idn_component = true;
    // Converted successfully. At this point the length of the output string
    // is original_length + output_length which may be shorter than the current
    // length of |out|. Trim |out| and ensure that the converted component can
    // be safely displayed to the user.
    out->resize(original_length + output_length);
    result.spoof_check_result = SpoofCheckIDNComponent(
        std::u16string_view(*out).substr(
            original_length, base::checked_cast<size_t>(output_length)),
        top_level_domain, top_level_domain_unicode);
    DCHECK_NE(IDNSpoofChecker::Result::kNone, result.spoof_check_result);
    if (ignore_spoof_check_results ||
        result.spoof_check_result == IDNSpoofChecker::Result::kSafe) {
      result.converted = true;
      return result;
    }
  }

  // We get here with no IDN or on error, in which case we just revert to
  // original string and append the literal input.
  out->resize(original_length);
  out->append(comp);
  return result;
}

// Returns true iff URL-parsing `spec` would reveal that it has the
// "view-source" scheme, and that parsing the spec minus that scheme also has
// the "view-source" scheme.
bool HasTwoViewSourceSchemes(std::string_view spec) {
  static constexpr char kViewSource[] = "view-source";
  url::Component scheme;
  if (!url::FindAndCompareScheme(spec.data(),
                                 base::checked_cast<int>(spec.size()),
                                 kViewSource, &scheme)) {
    return false;
  }
  // Consume the scheme.
  spec.remove_prefix(scheme.begin + scheme.len);
  // Consume the trailing colon. If it's not there, then `spec` didn't really
  // have the first view-source scheme.
  if (spec.empty() || spec[0] != ':')
    return false;
  spec.remove_prefix(1);

  return url::FindAndCompareScheme(
      spec.data(), base::checked_cast<int>(spec.size()), kViewSource, &scheme);
}

}  // namespace

const FormatUrlType kFormatUrlOmitNothing = 0;
const FormatUrlType kFormatUrlOmitUsernamePassword = 1 << 0;
const FormatUrlType kFormatUrlOmitHTTP = 1 << 1;
const FormatUrlType kFormatUrlOmitTrailingSlashOnBareHostname = 1 << 2;
const FormatUrlType kFormatUrlOmitHTTPS = 1 << 3;
const FormatUrlType kFormatUrlOmitTrivialSubdomains = 1 << 5;
const FormatUrlType kFormatUrlTrimAfterHost = 1 << 6;
const FormatUrlType kFormatUrlOmitFileScheme = 1 << 7;
const FormatUrlType kFormatUrlOmitMailToScheme = 1 << 8;
const FormatUrlType kFormatUrlOmitMobilePrefix = 1 << 9;

const FormatUrlType kFormatUrlOmitDefaults =
    kFormatUrlOmitUsernamePassword | kFormatUrlOmitHTTP |
    kFormatUrlOmitTrailingSlashOnBareHostname;

std::u16string FormatUrl(const GURL& url,
                         FormatUrlTypes format_types,
                         base::UnescapeRule::Type unescape_rules,
                         url::Parsed* new_parsed,
                         size_t* prefix_end,
                         size_t* offset_for_adjustment) {
  base::OffsetAdjuster::Adjustments adjustments;
  std::u16string result = FormatUrlWithAdjustments(
      url, format_types, unescape_rules, new_parsed, prefix_end, &adjustments);
  if (offset_for_adjustment) {
    base::OffsetAdjuster::AdjustOffset(adjustments, offset_for_adjustment,
                                       result.length());
  }
  return result;
}

std::u16string FormatUrlWithOffsets(
    const GURL& url,
    FormatUrlTypes format_types,
    base::UnescapeRule::Type unescape_rules,
    url::Parsed* new_parsed,
    size_t* prefix_end,
    std::vector<size_t>* offsets_for_adjustment) {
  base::OffsetAdjuster::Adjustments adjustments;
  const std::u16string& result = FormatUrlWithAdjustments(
      url, format_types, unescape_rules, new_parsed, prefix_end, &adjustments);
  base::OffsetAdjuster::AdjustOffsets(adjustments, offsets_for_adjustment,
                                      result.length());
  return result;
}

std::u16string FormatUrlWithAdjustments(
    const GURL& url,
    FormatUrlTypes format_types,
    base::UnescapeRule::Type unescape_rules,
    url::Parsed* new_parsed,
    size_t* prefix_end,
    base::OffsetAdjuster::Adjustments* adjustments) {
  DCHECK(adjustments);
  adjustments->clear();
  url::Parsed parsed_temp;
  if (!new_parsed)
    new_parsed = &parsed_temp;
  else
    *new_parsed = url::Parsed();

  // Special handling for view-source:. Don't use content::kViewSourceScheme
  // because this library shouldn't depend on chrome. Reject repeated
  // view-source schemes to avoid recursion.
  static constexpr std::string_view kViewSource = "view-source";
  if (url.SchemeIs(kViewSource) &&
      !HasTwoViewSourceSchemes(url.possibly_invalid_spec())) {
    return FormatViewSourceUrl(url, format_types, unescape_rules, new_parsed,
                               prefix_end, adjustments);
  }

  // We handle both valid and invalid URLs (this will give us the spec
  // regardless of validity).
  const std::string& spec = url.possibly_invalid_spec();
  const url::Parsed& parsed = url.parsed_for_possibly_invalid_spec();

  // Scheme & separators.  These are ASCII.
  size_t scheme_size = static_cast<size_t>(parsed.CountCharactersBefore(
      url::Parsed::USERNAME, true /* include_delimiter */));
  std::u16string url_string;
  url_string.insert(url_string.end(), spec.begin(), spec.begin() + scheme_size);
  new_parsed->scheme = parsed.scheme;

  // Username & password.
  if (((format_types & kFormatUrlOmitUsernamePassword) != 0) ||
      ((format_types & kFormatUrlTrimAfterHost) != 0)) {
    // Remove the username and password fields. We don't want to display those
    // to the user since they can be used for attacks,
    // e.g. "http://google.com:search@evil.ru/"
    new_parsed->username.reset();
    new_parsed->password.reset();
    // Update the adjustments based on removed username and/or password.
    if (parsed.username.is_nonempty() || parsed.password.is_nonempty()) {
      if (parsed.username.is_nonempty() && parsed.password.is_nonempty()) {
        // The seeming off-by-two is to account for the ':' after the username
        // and '@' after the password.
        adjustments->push_back(base::OffsetAdjuster::Adjustment(
            static_cast<size_t>(parsed.username.begin),
            static_cast<size_t>(parsed.username.len + parsed.password.len + 2),
            0));
      } else {
        const url::Component* nonempty_component =
            parsed.username.is_nonempty() ? &parsed.username : &parsed.password;
        // The seeming off-by-one is to account for the '@' after the
        // username/password.
        adjustments->push_back(base::OffsetAdjuster::Adjustment(
            static_cast<size_t>(nonempty_component->begin),
            static_cast<size_t>(nonempty_component->len + 1), 0));
      }
    }
  } else {
    AppendFormattedComponent(spec, parsed.username,
                             NonHostComponentTransform(unescape_rules),
                             &url_string, &new_parsed->username, adjustments);
    if (parsed.password.is_valid())
      url_string.push_back(':');
    AppendFormattedComponent(spec, parsed.password,
                             NonHostComponentTransform(unescape_rules),
                             &url_string, &new_parsed->password, adjustments);
    if (parsed.username.is_valid() || parsed.password.is_valid())
      url_string.push_back('@');
  }
  if (prefix_end)
    *prefix_end = static_cast<size_t>(url_string.length());

  // Host.
  bool trim_trivial_subdomains =
      (format_types & kFormatUrlOmitTrivialSubdomains) != 0;
  bool trim_mobile_prefix = (format_types & kFormatUrlOmitMobilePrefix) != 0;
  AppendFormattedComponent(
      spec, parsed.host,
      HostComponentTransform(trim_trivial_subdomains, trim_mobile_prefix),
      &url_string, &new_parsed->host, adjustments);

  // Port.
  if (parsed.port.is_nonempty()) {
    url_string.push_back(':');
    new_parsed->port.begin = url_string.length();
    url_string.insert(url_string.end(), spec.begin() + parsed.port.begin,
                      spec.begin() + parsed.port.end());
    new_parsed->port.len = url_string.length() - new_parsed->port.begin;
  } else {
    new_parsed->port.reset();
  }

  // Path & query.  Both get the same general unescape & convert treatment.
  if ((format_types & kFormatUrlTrimAfterHost) && url.IsStandard() &&
      !url.SchemeIsFile() && !url.SchemeIsFileSystem()) {
    size_t trimmed_length = parsed.path.len;
    // Remove query and the '?' delimeter.
    if (parsed.query.is_valid())
      trimmed_length += parsed.query.len + 1;

    // Remove ref and the '#" delimiter.
    if (parsed.ref.is_valid())
      trimmed_length += parsed.ref.len + 1;

    adjustments->push_back(
        base::OffsetAdjuster::Adjustment(parsed.path.begin, trimmed_length, 0));

  } else if ((format_types & kFormatUrlOmitTrailingSlashOnBareHostname) &&
             CanStripTrailingSlash(url)) {
    // Omit the path, which is a single trailing slash. There's no query or ref.
    if (parsed.path.is_nonempty()) {
      adjustments->push_back(base::OffsetAdjuster::Adjustment(
          parsed.path.begin, parsed.path.len, 0));
    }
  } else {
    // Append the formatted path, query, and ref.
    AppendFormattedComponent(spec, parsed.path,
                             NonHostComponentTransform(unescape_rules),
                             &url_string, &new_parsed->path, adjustments);

    if (parsed.query.is_valid())
      url_string.push_back('?');
    AppendFormattedComponent(spec, parsed.query,
                             NonHostComponentTransform(unescape_rules),
                             &url_string, &new_parsed->query, adjustments);

    if (parsed.ref.is_valid())
      url_string.push_back('#');
    AppendFormattedComponent(spec, parsed.ref,
                             NonHostComponentTransform(unescape_rules),
                             &url_string, &new_parsed->ref, adjustments);
  }

  // url_formatter::FixupURL() treats "ftp.foo.com" as ftp://ftp.foo.com.  This
  // means that if we trim the scheme off a URL whose host starts with "ftp."
  // and the user inputs this into any field subject to fixup (which is
  // basically all input fields), the meaning would be changed.  (In fact, often
  // the formatted URL is directly pre-filled into an input field.)  For this
  // reason we avoid stripping schemes in this case.
  const char kFTP[] = "ftp.";
  bool strip_scheme =
      !base::StartsWith(url.host(), kFTP, base::CompareCase::SENSITIVE) &&
      (((format_types & kFormatUrlOmitHTTP) &&
        url.SchemeIs(url::kHttpScheme)) ||
       ((format_types & kFormatUrlOmitHTTPS) &&
        url.SchemeIs(url::kHttpsScheme)) ||
       ((format_types & kFormatUrlOmitFileScheme) &&
        url.SchemeIs(url::kFileScheme)) ||
       ((format_types & kFormatUrlOmitMailToScheme) &&
        url.SchemeIs(url::kMailToScheme)));

  // If we need to strip out schemes do it after the fact.
  if (strip_scheme) {
    DCHECK(new_parsed->scheme.is_valid());
    size_t scheme_and_separator_len =
        url.SchemeIs(url::kMailToScheme)
            ? new_parsed->scheme.len + 1   // +1 for :.
            : new_parsed->scheme.len + 3;  // +3 for ://.
#if BUILDFLAG(IS_WIN)
    // Because there's an additional leading slash after the scheme for local
    // files on Windows, we should remove it for URL display when eliding
    // the scheme by offsetting by an additional character.
    if (url.SchemeIs(url::kFileScheme) &&
        base::StartsWith(url_string, u"file:///",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      ++new_parsed->path.begin;
      ++scheme_size;
      ++scheme_and_separator_len;
    }
#endif

    url_string.erase(0, scheme_size);
    // Because offsets in the |adjustments| are already calculated with respect
    // to the string with the http:// prefix in it, those offsets remain correct
    // after stripping the prefix.  The only thing necessary is to add an
    // adjustment to reflect the stripped prefix.
    adjustments->insert(adjustments->begin(),
                        base::OffsetAdjuster::Adjustment(0, scheme_size, 0));

    if (prefix_end)
      *prefix_end -= scheme_size;

    // Adjust new_parsed.
    new_parsed->scheme.reset();
    AdjustAllComponentsButScheme(-scheme_and_separator_len, new_parsed);
  }

  return url_string;
}

bool CanStripTrailingSlash(const GURL& url) {
  // Omit the path only for standard, non-file URLs with nothing but "/" after
  // the hostname.
  return url.IsStandard() && !url.SchemeIsFile() && !url.SchemeIsFileSystem() &&
         !url.has_query() && !url.has_ref() && url.path_piece() == "/";
}

void AppendFormattedHost(const GURL& url, std::u16string* output) {
  AppendFormattedComponent(
      url.possibly_invalid_spec(), url.parsed_for_possibly_invalid_spec().host,
      HostComponentTransform(false, false), output, nullptr, nullptr);
}

IDNConversionResult UnsafeIDNToUnicodeWithDetails(std::string_view host) {
  return UnsafeIDNToUnicodeWithAdjustments(host, nullptr);
}

std::u16string IDNToUnicode(std::string_view host) {
  return IDNToUnicodeWithAdjustments(host, nullptr).result;
}

std::string StripWWW(const std::string& text) {
  // Exclude the registry and domain from trivial subdomain stripping.
  std::string domain_and_registry =
      net::registry_controlled_domains::GetDomainAndRegistry(
          text, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  // If there is no domain and registry, we may be looking at an intranet
  // or otherwise non-standard host. Leave those alone.
  if (domain_and_registry.empty())
    return text;
  return text.size() - domain_and_registry.length() >= kWwwLength &&
                 base::StartsWith(text, kWww, base::CompareCase::SENSITIVE)
             ? text.substr(kWwwLength)
             : text;
}

void StripWWWFromHostComponent(const std::string& url, url::Component* host) {
  std::string host_str = url.substr(host->begin, host->len);
  if (StripWWW(host_str) == host_str)
    return;
  host->begin += kWwwLength;
  host->len -= kWwwLength;
}

std::string StripMobilePrefix(const std::string& text) {
  return text.size() >= kMobilePrefixLength &&
                 base::StartsWith(text, kMobilePrefix,
                                  base::CompareCase::SENSITIVE)
             ? text.substr(kMobilePrefixLength)
             : text;
}

Skeletons GetSkeletons(const std::u16string& host) {
  return g_idn_spoof_checker.Get().GetSkeletons(host);
}

TopDomainEntry LookupSkeletonInTopDomains(const std::string& skeleton,
                                          const SkeletonType type) {
  return g_idn_spoof_checker.Get().LookupSkeletonInTopDomains(skeleton, type);
}

std::u16string MaybeRemoveDiacritics(const std::u16string& host) {
  return g_idn_spoof_checker.Get().MaybeRemoveDiacritics(host);
}

IDNA2008DeviationCharacter GetDeviationCharacter(std::u16string_view hostname) {
  return g_idn_spoof_checker.Get().GetDeviationCharacter(hostname);
}

}  // namespace url_formatter
