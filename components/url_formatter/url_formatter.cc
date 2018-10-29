// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/url_formatter.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_local_storage.h"
#include "build/build_config.h"
#include "components/url_formatter/idn_spoof_checker.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/icu/source/common/unicode/uidna.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

namespace url_formatter {

namespace {

enum IDNConversionStatus {
  // The input has no IDN component.
  NO_IDN,
  // The input has an IDN component but it's unsafe to display
  // so was not converted.
  UNSAFE_IDN,
  // The input was successfully converted to IDN.
  IDN_CONVERTED,
};

IDNConversionResult IDNToUnicodeWithAdjustments(
    base::StringPiece host,
    base::OffsetAdjuster::Adjustments* adjustments);

IDNConversionStatus IDNToUnicodeOneComponent(const base::char16* comp,
                                             size_t comp_len,
                                             bool is_tld_ascii,
                                             base::string16* out);

class AppendComponentTransform {
 public:
  AppendComponentTransform() {}
  virtual ~AppendComponentTransform() {}

  virtual base::string16 Execute(
      const std::string& component_text,
      base::OffsetAdjuster::Adjustments* adjustments) const = 0;

  // NOTE: No DISALLOW_COPY_AND_ASSIGN here, since gcc < 4.3.0 requires an
  // accessible copy constructor in order to call AppendFormattedComponent()
  // with an inline temporary (see http://gcc.gnu.org/bugs/#cxx%5Frvalbind ).
};

class HostComponentTransform : public AppendComponentTransform {
 public:
  HostComponentTransform(bool trim_trivial_subdomains)
      : trim_trivial_subdomains_(trim_trivial_subdomains) {}

 private:
  base::string16 Execute(
      const std::string& component_text,
      base::OffsetAdjuster::Adjustments* adjustments) const override {
    if (!trim_trivial_subdomains_)
      return IDNToUnicodeWithAdjustments(component_text, adjustments).result;

    // Exclude the registry and domain from trivial subdomain stripping.
    // To get the adjustment offset calculations correct, we need to transform
    // the registry and domain portion of the host as well.
    std::string domain_and_registry =
        net::registry_controlled_domains::GetDomainAndRegistry(
            component_text,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

    // If there is no domain and registry, we may be looking at an intranet
    // or otherwise non-standard host. Leave those alone.
    if (domain_and_registry.empty())
      return IDNToUnicodeWithAdjustments(component_text, adjustments).result;

    base::OffsetAdjuster::Adjustments trivial_subdomains_adjustments;
    std::string transformed_host = component_text;
    constexpr char kWww[] = "www.";
    constexpr size_t kWwwLength = 4;
    if (component_text.size() - domain_and_registry.length() >= kWwwLength &&
        StartsWith(component_text, kWww, base::CompareCase::SENSITIVE)) {
      transformed_host.erase(0, kWwwLength);
      trivial_subdomains_adjustments.push_back(
          base::OffsetAdjuster::Adjustment(0, kWwwLength, 0));
    }

    base::string16 unicode_result =
        IDNToUnicodeWithAdjustments(transformed_host, adjustments).result;
    base::OffsetAdjuster::MergeSequentialAdjustments(
        trivial_subdomains_adjustments, adjustments);
    return unicode_result;
  }

  bool trim_trivial_subdomains_;
};

class NonHostComponentTransform : public AppendComponentTransform {
 public:
  explicit NonHostComponentTransform(net::UnescapeRule::Type unescape_rules)
      : unescape_rules_(unescape_rules) {}

 private:
  base::string16 Execute(
      const std::string& component_text,
      base::OffsetAdjuster::Adjustments* adjustments) const override {
    return (unescape_rules_ == net::UnescapeRule::NONE)
               ? base::UTF8ToUTF16WithAdjustments(component_text, adjustments)
               : net::UnescapeAndDecodeUTF8URLComponentWithAdjustments(
                     component_text, unescape_rules_, adjustments);
  }

  const net::UnescapeRule::Type unescape_rules_;
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
                              base::string16* output,
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
    for (auto comp_iter = component_transform_adjustments.begin();
         comp_iter != component_transform_adjustments.end(); ++comp_iter)
      comp_iter->original_offset += original_component_begin;
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
base::string16 FormatViewSourceUrl(
    const GURL& url,
    FormatUrlTypes format_types,
    net::UnescapeRule::Type unescape_rules,
    url::Parsed* new_parsed,
    size_t* prefix_end,
    base::OffsetAdjuster::Adjustments* adjustments) {
  DCHECK(new_parsed);
  const char kViewSource[] = "view-source:";
  const size_t kViewSourceLength = arraysize(kViewSource) - 1;

  // Format the underlying URL and record adjustments.
  const std::string& url_str(url.possibly_invalid_spec());
  adjustments->clear();
  base::string16 result(
      base::ASCIIToUTF16(kViewSource) +
      FormatUrlWithAdjustments(GURL(url_str.substr(kViewSourceLength)),
                               format_types, unescape_rules, new_parsed,
                               prefix_end, adjustments));
  // Revise |adjustments| by shifting to the offsets to prefix that the above
  // call to FormatUrl didn't get to see.
  for (auto it = adjustments->begin(); it != adjustments->end(); ++it)
    it->original_offset += kViewSourceLength;

  // Adjust positions of the parsed components.
  if (new_parsed->scheme.is_nonempty()) {
    // Assume "view-source:real-scheme" as a scheme.
    new_parsed->scheme.len += kViewSourceLength;
  } else {
    new_parsed->scheme.begin = 0;
    new_parsed->scheme.len = kViewSourceLength - 1;
  }
  AdjustAllComponentsButScheme(kViewSourceLength, new_parsed);

  if (prefix_end)
    *prefix_end += kViewSourceLength;

  return result;
}

base::LazyInstance<IDNSpoofChecker>::Leaky g_idn_spoof_checker =
    LAZY_INSTANCE_INITIALIZER;

// TODO(brettw): We may want to skip this step in the case of file URLs to
// allow unicode UNC hostnames regardless of encodings.
IDNConversionResult IDNToUnicodeWithAdjustments(
    base::StringPiece host,
    base::OffsetAdjuster::Adjustments* adjustments) {
  if (adjustments)
    adjustments->clear();
  // Convert the ASCII input to a base::string16 for ICU.
  base::string16 input16;
  input16.reserve(host.length());
  input16.insert(input16.end(), host.begin(), host.end());

  bool is_tld_ascii = true;
  size_t last_dot = host.rfind('.');
  if (last_dot != base::StringPiece::npos &&
      host.substr(last_dot).starts_with(".xn--")) {
    is_tld_ascii = false;
  }

  IDNConversionResult result;
  // Do each component of the host separately, since we enforce script matching
  // on a per-component basis.
  base::string16 out16;
  for (size_t component_start = 0, component_end;
       component_start < input16.length();
       component_start = component_end + 1) {
    // Find the end of the component.
    component_end = input16.find('.', component_start);
    if (component_end == base::string16::npos)
      component_end = input16.length();  // For getting the last component.
    size_t component_length = component_end - component_start;
    size_t new_component_start = out16.length();
    bool converted_idn = false;
    if (component_end > component_start) {
      // Add the substring that we just found.
      IDNConversionStatus status =
          IDNToUnicodeOneComponent(input16.data() + component_start,
                                   component_length, is_tld_ascii, &out16);
      converted_idn = (status == IDN_CONVERTED);
      result.has_idn_component |=
          (status == IDN_CONVERTED || status == UNSAFE_IDN);
    }
    size_t new_component_length = out16.length() - new_component_start;

    if (converted_idn && adjustments) {
      adjustments->push_back(base::OffsetAdjuster::Adjustment(
          component_start, component_length, new_component_length));
    }

    // Need to add the dot we just found (if we found one).
    if (component_end < input16.length())
      out16.push_back('.');
  }

  result.result = out16;

  // Leave as punycode any inputs that spoof top domains.
  if (result.has_idn_component) {
    result.matching_top_domain =
        g_idn_spoof_checker.Get().GetSimilarTopDomain(out16);
    if (!result.matching_top_domain.empty()) {
      if (adjustments)
        adjustments->clear();
      result.result = input16;
    }
  }

  return result;
}

// Returns true if the given Unicode host component is safe to display to the
// user. Note that this function does not deal with pure ASCII domain labels at
// all even though it's possible to make up look-alike labels with ASCII
// characters alone.
bool IsIDNComponentSafe(base::StringPiece16 label, bool is_tld_ascii) {
  return g_idn_spoof_checker.Get().SafeToDisplayAsUnicode(label, is_tld_ascii);
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
    if (U_FAILURE(err))
      value = nullptr;
  }

  UIDNA* value;
};

base::LazyInstance<UIDNAWrapper>::Leaky g_uidna = LAZY_INSTANCE_INITIALIZER;

// Converts one component (label) of a host (between dots) to Unicode if safe.
// The result will be APPENDED to the given output string and will be the
// same as the input if it is not IDN in ACE/punycode or the IDN is unsafe to
// display.
// Returns whether any conversion was performed.
IDNConversionStatus IDNToUnicodeOneComponent(const base::char16* comp,
                                             size_t comp_len,
                                             bool is_tld_ascii,
                                             base::string16* out) {
  DCHECK(out);
  if (comp_len == 0)
    return NO_IDN;

  IDNConversionStatus idn_status = NO_IDN;
  // Only transform if the input can be an IDN component.
  static const base::char16 kIdnPrefix[] = {'x', 'n', '-', '-'};
  if ((comp_len > arraysize(kIdnPrefix)) &&
      !memcmp(comp, kIdnPrefix, sizeof(kIdnPrefix))) {
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
          uidna, comp, static_cast<int32_t>(comp_len), &(*out)[original_length],
          output_length, &info, &status);
    } while ((status == U_BUFFER_OVERFLOW_ERROR && info.errors == 0));

    if (U_SUCCESS(status) && info.errors == 0) {
      // Converted successfully. Ensure that the converted component
      // can be safely displayed to the user.
      out->resize(original_length + output_length);
      if (IsIDNComponentSafe(
              base::StringPiece16(out->data() + original_length,
                                  base::checked_cast<size_t>(output_length)),
              is_tld_ascii)) {
        return IDN_CONVERTED;
      }
      idn_status = UNSAFE_IDN;
    }

    // Something went wrong. Revert to original string.
    out->resize(original_length);
  }

  // We get here with no IDN or on error, in which case we just append the
  // literal input.
  out->append(comp, comp_len);
  return idn_status;
}

}  // namespace

const FormatUrlType kFormatUrlOmitNothing = 0;
const FormatUrlType kFormatUrlOmitUsernamePassword = 1 << 0;
const FormatUrlType kFormatUrlOmitHTTP = 1 << 1;
const FormatUrlType kFormatUrlOmitTrailingSlashOnBareHostname = 1 << 2;
const FormatUrlType kFormatUrlOmitHTTPS = 1 << 3;
const FormatUrlType kFormatUrlExperimentalElideAfterHost = 1 << 4;
const FormatUrlType kFormatUrlOmitTrivialSubdomains = 1 << 5;
const FormatUrlType kFormatUrlTrimAfterHost = 1 << 6;
const FormatUrlType kFormatUrlOmitFileScheme = 1 << 7;

const FormatUrlType kFormatUrlOmitDefaults =
    kFormatUrlOmitUsernamePassword | kFormatUrlOmitHTTP |
    kFormatUrlOmitTrailingSlashOnBareHostname;

base::string16 FormatUrl(const GURL& url,
                         FormatUrlTypes format_types,
                         net::UnescapeRule::Type unescape_rules,
                         url::Parsed* new_parsed,
                         size_t* prefix_end,
                         size_t* offset_for_adjustment) {
  base::OffsetAdjuster::Adjustments adjustments;
  base::string16 result = FormatUrlWithAdjustments(
      url, format_types, unescape_rules, new_parsed, prefix_end, &adjustments);
  if (offset_for_adjustment) {
    base::OffsetAdjuster::AdjustOffset(adjustments, offset_for_adjustment,
                                       result.length());
  }
  return result;
}

base::string16 FormatUrlWithOffsets(
    const GURL& url,
    FormatUrlTypes format_types,
    net::UnescapeRule::Type unescape_rules,
    url::Parsed* new_parsed,
    size_t* prefix_end,
    std::vector<size_t>* offsets_for_adjustment) {
  base::OffsetAdjuster::Adjustments adjustments;
  const base::string16& result = FormatUrlWithAdjustments(
      url, format_types, unescape_rules, new_parsed, prefix_end, &adjustments);
  base::OffsetAdjuster::AdjustOffsets(adjustments, offsets_for_adjustment,
                                      result.length());
  return result;
}

base::string16 FormatUrlWithAdjustments(
    const GURL& url,
    FormatUrlTypes format_types,
    net::UnescapeRule::Type unescape_rules,
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

  // Special handling for view-source:.  Don't use content::kViewSourceScheme
  // because this library shouldn't depend on chrome.
  const char kViewSource[] = "view-source";
  // Reject "view-source:view-source:..." to avoid deep recursion.
  const char kViewSourceTwice[] = "view-source:view-source:";
  if (url.SchemeIs(kViewSource) &&
      !base::StartsWith(url.possibly_invalid_spec(), kViewSourceTwice,
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return FormatViewSourceUrl(url, format_types, unescape_rules,
                               new_parsed, prefix_end, adjustments);
  }

  // We handle both valid and invalid URLs (this will give us the spec
  // regardless of validity).
  const std::string& spec = url.possibly_invalid_spec();
  const url::Parsed& parsed = url.parsed_for_possibly_invalid_spec();

  // Scheme & separators.  These are ASCII.
  size_t scheme_size = static_cast<size_t>(parsed.CountCharactersBefore(
      url::Parsed::USERNAME, true /* include_delimiter */));
  base::string16 url_string;
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
  AppendFormattedComponent(spec, parsed.host,
                           HostComponentTransform(trim_trivial_subdomains),
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
  if ((format_types & kFormatUrlTrimAfterHost) ||
      ((format_types & kFormatUrlExperimentalElideAfterHost) &&
       url.IsStandard() && !url.SchemeIsFile() && !url.SchemeIsFileSystem())) {
    // Only elide when the eliding is required and when the host is followed by
    // more than just one forward slash.
    bool should_elide = (format_types & kFormatUrlExperimentalElideAfterHost) &&
                        ((parsed.path.len > 1) || parsed.query.is_valid() ||
                         parsed.ref.is_valid());

    // Either remove the path completely, or, if eliding, keep the first slash.
    const size_t new_path_len = should_elide ? 1 : 0;

    size_t trimmed_length = parsed.path.len - new_path_len;
    // Remove query and the '?' delimeter.
    if (parsed.query.is_valid())
      trimmed_length += parsed.query.len + 1;

    // Remove ref and the '#" delimiter.
    if (parsed.ref.is_valid())
      trimmed_length += parsed.ref.len + 1;

    if (should_elide) {
      // Replace everything after the host with a forward slash and ellipsis.
      url_string.push_back('/');
      constexpr base::char16 kEllipsisUTF16[] = {0x2026, 0};
      url_string.append(kEllipsisUTF16);
    }

    adjustments->push_back(base::OffsetAdjuster::Adjustment(
        parsed.path.begin + new_path_len, trimmed_length, new_path_len));

  } else if ((format_types & kFormatUrlOmitTrailingSlashOnBareHostname) &&
             CanStripTrailingSlash(url)) {
    // Omit the path, which is a single trailing slash. There's no query or ref.
    if (parsed.path.len > 0) {
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
        url.SchemeIs(url::kFileScheme)));

  // If we need to strip out schemes do it after the fact.
  if (strip_scheme) {
    DCHECK(new_parsed->scheme.is_valid());
    size_t scheme_and_separator_len =
        new_parsed->scheme.len + 3;  // +3 for ://.
#if defined(OS_WIN)
    // Because there's an additional leading slash after the scheme for local
    // files on Windows, we should remove it for URL display when eliding
    // the scheme by offsetting by an additional character.
    if (url.SchemeIs(url::kFileScheme) &&
        base::StartsWith(url_string, base::ASCIIToUTF16("file:///"),
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

void AppendFormattedHost(const GURL& url, base::string16* output) {
  AppendFormattedComponent(
      url.possibly_invalid_spec(), url.parsed_for_possibly_invalid_spec().host,
      HostComponentTransform(false), output, nullptr, nullptr);
}

IDNConversionResult IDNToUnicodeWithDetails(base::StringPiece host) {
  return IDNToUnicodeWithAdjustments(host, nullptr);
}

base::string16 IDNToUnicode(base::StringPiece host) {
  return IDNToUnicodeWithAdjustments(host, nullptr).result;
}

base::string16 StripWWW(const base::string16& text) {
  const base::string16 www(base::ASCIIToUTF16("www."));
  return base::StartsWith(text, www, base::CompareCase::SENSITIVE)
      ? text.substr(www.length()) : text;
}

base::string16 StripWWWFromHost(const GURL& url) {
  DCHECK(url.is_valid());
  return StripWWW(base::ASCIIToUTF16(url.host_piece()));
}

Skeletons GetSkeletons(const base::string16& host) {
  return g_idn_spoof_checker.Get().GetSkeletons(host);
}

}  // namespace url_formatter
