// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_input.h"

#include <vector>

#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/url_formatter/url_fixer.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/url_canon_ip.h"
#include "url/url_util.h"

namespace {

// Hardcode constant to avoid any dependencies on content/.
const char kViewSourceScheme[] = "view-source";
const char kDevToolsScheme[] = "devtools";

void AdjustCursorPositionIfNecessary(size_t num_leading_chars_removed,
                                     size_t* cursor_position) {
  if (*cursor_position == base::string16::npos)
    return;
  if (num_leading_chars_removed < *cursor_position)
    *cursor_position -= num_leading_chars_removed;
  else
    *cursor_position = 0;
}

// Finds all terms in |text| that start with http:// or https:// plus at least
// one more character and puts the text after the prefix in
// |terms_prefixed_by_http_or_https|.
void PopulateTermsPrefixedByHttpOrHttps(
    const base::string16& text,
    std::vector<base::string16>* terms_prefixed_by_http_or_https) {
  // Split on whitespace rather than use ICU's word iterator because, for
  // example, ICU's iterator may break on punctuation (such as ://) or decide
  // to split a single term in a hostname (if it seems to think that the
  // hostname is multiple words).  Neither of these behaviors is desirable.
  const std::string separator(url::kStandardSchemeSeparator);
  for (const auto& term :
       base::SplitString(text, base::ASCIIToUTF16(" "),
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    const std::string term_utf8(base::UTF16ToUTF8(term));
    static const char* kSchemes[2] = { url::kHttpScheme, url::kHttpsScheme };
    for (const char* scheme : kSchemes) {
      const std::string prefix(scheme + separator);
      // Doing an ASCII comparison is okay because prefix is ASCII.
      if (base::StartsWith(term_utf8, prefix,
                           base::CompareCase::INSENSITIVE_ASCII) &&
          (term_utf8.length() > prefix.length())) {
        terms_prefixed_by_http_or_https->push_back(
            term.substr(prefix.length()));
      }
    }
  }
}

}  // namespace

AutocompleteInput::AutocompleteInput()
    : cursor_position_(base::string16::npos),
      current_page_classification_(metrics::OmniboxEventProto::INVALID_SPEC),
      type_(metrics::OmniboxInputType::EMPTY),
      prevent_inline_autocomplete_(false),
      prefer_keyword_(false),
      allow_exact_keyword_match_(true),
      keyword_mode_entry_method_(metrics::OmniboxEventProto::INVALID),
      want_asynchronous_matches_(true),
      from_omnibox_focus_(false) {}

AutocompleteInput::AutocompleteInput(
    const base::string16& text,
    metrics::OmniboxEventProto::PageClassification current_page_classification,
    const AutocompleteSchemeClassifier& scheme_classifier)
    : AutocompleteInput(text,
                        std::string::npos,
                        current_page_classification,
                        scheme_classifier) {}

AutocompleteInput::AutocompleteInput(
    const base::string16& text,
    size_t cursor_position,
    metrics::OmniboxEventProto::PageClassification current_page_classification,
    const AutocompleteSchemeClassifier& scheme_classifier)
    : AutocompleteInput(text,
                        cursor_position,
                        "",
                        current_page_classification,
                        scheme_classifier) {}

AutocompleteInput::AutocompleteInput(
    const base::string16& text,
    size_t cursor_position,
    const std::string& desired_tld,
    metrics::OmniboxEventProto::PageClassification current_page_classification,
    const AutocompleteSchemeClassifier& scheme_classifier)
    : AutocompleteInput() {
  cursor_position_ = cursor_position;
  current_page_classification_ = current_page_classification;
  desired_tld_ = desired_tld;
  Init(text, scheme_classifier);
}

void AutocompleteInput::Init(
    const base::string16& text,
    const AutocompleteSchemeClassifier& scheme_classifier) {
  DCHECK(cursor_position_ <= text.length() ||
         cursor_position_ == base::string16::npos)
      << "Text: '" << text << "', cp: " << cursor_position_;
  // None of the providers care about leading white space so we always trim it.
  // Providers that care about trailing white space handle trimming themselves.
  if ((base::TrimWhitespace(text, base::TRIM_LEADING, &text_) &
       base::TRIM_LEADING) != 0)
    AdjustCursorPositionIfNecessary(text.length() - text_.length(),
                                    &cursor_position_);

  GURL canonicalized_url;
  type_ = Parse(text_, desired_tld_, scheme_classifier, &parts_, &scheme_,
                &canonicalized_url);
  PopulateTermsPrefixedByHttpOrHttps(text_, &terms_prefixed_by_http_or_https_);

  if (((type_ == metrics::OmniboxInputType::UNKNOWN) ||
       (type_ == metrics::OmniboxInputType::URL)) &&
      canonicalized_url.is_valid() &&
      (!canonicalized_url.IsStandard() || canonicalized_url.SchemeIsFile() ||
       canonicalized_url.SchemeIsFileSystem() ||
       !canonicalized_url.host().empty()))
    canonicalized_url_ = canonicalized_url;
}

AutocompleteInput::AutocompleteInput(const AutocompleteInput& other) = default;

AutocompleteInput::~AutocompleteInput() {
}

// static
std::string AutocompleteInput::TypeToString(metrics::OmniboxInputType type) {
  switch (type) {
    case metrics::OmniboxInputType::EMPTY:
      return "invalid";
    case metrics::OmniboxInputType::UNKNOWN:
      return "unknown";
    case metrics::OmniboxInputType::DEPRECATED_REQUESTED_URL:
      return "deprecated-requested-url";
    case metrics::OmniboxInputType::URL:
      return "url";
    case metrics::OmniboxInputType::QUERY:
      return "query";
    case metrics::OmniboxInputType::DEPRECATED_FORCED_QUERY:
      return "deprecated-forced-query";
  }
  return std::string();
}

// static
metrics::OmniboxInputType AutocompleteInput::Parse(
    const base::string16& text,
    const std::string& desired_tld,
    const AutocompleteSchemeClassifier& scheme_classifier,
    url::Parsed* parts,
    base::string16* scheme,
    GURL* canonicalized_url) {
  size_t first_non_white = text.find_first_not_of(base::kWhitespaceUTF16, 0);
  if (first_non_white == base::string16::npos)
    return metrics::OmniboxInputType::EMPTY;  // All whitespace.

  // Ask our parsing back-end to help us understand what the user typed.  We
  // use the URLFixerUpper here because we want to be smart about what we
  // consider a scheme.  For example, we shouldn't consider www.google.com:80
  // to have a scheme.
  url::Parsed local_parts;
  if (!parts)
    parts = &local_parts;
  const base::string16 parsed_scheme(url_formatter::SegmentURL(text, parts));
  if (scheme)
    *scheme = parsed_scheme;
  const std::string parsed_scheme_utf8(base::UTF16ToUTF8(parsed_scheme));

  // If we can't canonicalize the user's input, the rest of the autocomplete
  // system isn't going to be able to produce a navigable URL match for it.
  // So we just return QUERY immediately in these cases.
  GURL placeholder_canonicalized_url;
  if (!canonicalized_url)
    canonicalized_url = &placeholder_canonicalized_url;
  *canonicalized_url =
      url_formatter::FixupURL(base::UTF16ToUTF8(text), desired_tld);
  if (!canonicalized_url->is_valid())
    return metrics::OmniboxInputType::QUERY;

  if (base::LowerCaseEqualsASCII(parsed_scheme_utf8, url::kFileScheme)) {
    // A user might or might not type a scheme when entering a file URL.  In
    // either case, |parsed_scheme_utf8| will tell us that this is a file URL,
    // but |parts->scheme| might be empty, e.g. if the user typed "C:\foo".
    return metrics::OmniboxInputType::URL;
  }

  if (base::LowerCaseEqualsASCII(parsed_scheme_utf8, kDevToolsScheme)) {
    // A user might type in the fallback url when using devtools.  In
    // this case, |parsed_scheme_utf8| will tell us that this is a devtools URL,
    // but |parts->scheme| might be empty.e.g. if the user typed
    // "chrome-devtools://".
    return metrics::OmniboxInputType::URL;
  }

  // Treat javascript: scheme queries followed by things that are unlikely to
  // be code as UNKNOWN, rather than script to execute (URL).
  if (RE2::FullMatch(base::UTF16ToUTF8(text), "(?i)javascript:([^;=().\"]*)")) {
    return metrics::OmniboxInputType::UNKNOWN;
  }

  // If the user typed a scheme, and it's HTTP or HTTPS, we know how to parse it
  // well enough that we can fall through to the heuristics below.  If it's
  // something else, we can just determine our action based on what we do with
  // any input of this scheme.  In theory we could do better with some schemes
  // (e.g. "ftp" or "view-source") but I'll wait to spend the effort on that
  // until I run into some cases that really need it.
  if (parts->scheme.is_nonempty() &&
      !base::LowerCaseEqualsASCII(parsed_scheme_utf8, url::kHttpScheme) &&
      !base::LowerCaseEqualsASCII(parsed_scheme_utf8, url::kHttpsScheme)) {
    metrics::OmniboxInputType type =
        scheme_classifier.GetInputTypeForScheme(parsed_scheme_utf8);
    if (type != metrics::OmniboxInputType::EMPTY)
      return type;

    // We don't know about this scheme.  It might be that the user typed a
    // URL of the form "username:password@foo.com".
    const base::string16 http_scheme_prefix =
        base::ASCIIToUTF16(std::string(url::kHttpScheme) +
                           url::kStandardSchemeSeparator);
    url::Parsed http_parts;
    base::string16 http_scheme;
    GURL http_canonicalized_url;
    metrics::OmniboxInputType http_type =
        Parse(http_scheme_prefix + text, desired_tld, scheme_classifier,
              &http_parts, &http_scheme, &http_canonicalized_url);
    DCHECK_EQ(std::string(url::kHttpScheme),
              base::UTF16ToUTF8(http_scheme));

    if ((http_type == metrics::OmniboxInputType::URL) &&
        http_parts.username.is_nonempty() &&
        http_parts.password.is_nonempty()) {
      // Manually re-jigger the parsed parts to match |text| (without the
      // http scheme added).
      http_parts.scheme.reset();
      url::Component* components[] = {
        &http_parts.username,
        &http_parts.password,
        &http_parts.host,
        &http_parts.port,
        &http_parts.path,
        &http_parts.query,
        &http_parts.ref,
      };
      for (size_t i = 0; i < base::size(components); ++i) {
        url_formatter::OffsetComponent(
            -static_cast<int>(http_scheme_prefix.length()), components[i]);
      }

      *parts = http_parts;
      if (scheme)
        scheme->clear();
      *canonicalized_url = http_canonicalized_url;

      return metrics::OmniboxInputType::URL;
    }

    // We don't know about this scheme and it doesn't look like the user
    // typed a username and password.  It's likely to be a search operator
    // like "site:" or "link:".  We classify it as UNKNOWN so the user has
    // the option of treating it as a URL if we're wrong.
    // Note that SegmentURL() is smart so we aren't tricked by "c:\foo" or
    // "www.example.com:81" in this case.
    return metrics::OmniboxInputType::UNKNOWN;
  }

  // Either the user didn't type a scheme, in which case we need to distinguish
  // between an HTTP URL and a query, or the scheme is HTTP or HTTPS, in which
  // case we should reject invalid formulations.

  // Determine the host family.  We get this information by (re-)canonicalizing
  // the already-canonicalized host rather than using the user's original input,
  // in case fixup affected the result here (e.g. an input that looks like an
  // IPv4 address but with a non-empty desired TLD would return IPV4 before
  // fixup and NEUTRAL afterwards, and we want to treat it as NEUTRAL).
  url::CanonHostInfo host_info;
  net::CanonicalizeHost(canonicalized_url->host(), &host_info);

  // Check if the canonicalized host has a known TLD, which we'll want to know
  // below.
  const size_t registry_length =
      net::registry_controlled_domains::GetCanonicalHostRegistryLength(
          canonicalized_url->host(),
          net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  DCHECK_NE(std::string::npos, registry_length);
  const bool has_known_tld = registry_length != 0;

  // See if the hostname is valid.  While IE and GURL allow hostnames to contain
  // many other characters (perhaps for weird intranet machines), it's extremely
  // unlikely that a user would be trying to type those in for anything other
  // than a search query.
  //
  // Per https://tools.ietf.org/html/rfc6761, the .invalid TLD is considered
  // non-navigable and thus is treated like a non-compliant hostname. (Though
  // just the word "invalid" is not a hostname).
  const base::string16 original_host(
      text.substr(parts->host.begin, parts->host.len));
  if (text != base::ASCIIToUTF16("invalid") &&
      (host_info.family == url::CanonHostInfo::NEUTRAL) &&
      (!net::IsCanonicalizedHostCompliant(canonicalized_url->host()) ||
       canonicalized_url->DomainIs("invalid"))) {
    // Invalid hostname.  There are several possible cases:
    // * The user is typing a multi-word query.  If we see a space anywhere in
    //   the input host we assume this is a search and return QUERY.  (We check
    //   the input string instead of canonicalized_url->host() in case fixup
    //   escaped the space.)
    // * The user is typing some garbage string.  Return QUERY.
    // * Our checker is too strict and the user is typing a real-world URL
    //   that's "invalid" but resolves.  To catch these, we return UNKNOWN when
    //   the user explicitly typed a scheme or when the hostname has a known
    //   TLD, so we'll still search by default but we'll show the accidental
    //   search infobar if necessary.
    //
    // This means we would block the following kinds of navigation attempts:
    // * Navigations to a hostname with spaces
    // * Navigations to a hostname with invalid characters and an unknown TLD
    // These might be possible in intranets, but we're not going to support them
    // without concrete evidence that doing so is necessary.
    return (parts->scheme.is_nonempty() ||
            (has_known_tld &&
             (original_host.find(' ') == base::string16::npos)))
               ? metrics::OmniboxInputType::UNKNOWN
               : metrics::OmniboxInputType::QUERY;
  }

  // For hostnames that look like IP addresses, distinguish between IPv6
  // addresses, which are basically guaranteed to be navigations, and IPv4
  // addresses, which are much fuzzier.
  if (host_info.family == url::CanonHostInfo::IPV6)
    return metrics::OmniboxInputType::URL;
  if (host_info.family == url::CanonHostInfo::IPV4) {
    // The host may be a real IP address, or something that looks a bit like it
    // (e.g. "1.2" or "3232235521").  We check whether it was convertible to an
    // IP with a non-zero first octet; IPs with first octet zero are "source
    // IPs" and are almost never navigable as destination addresses.
    //
    // The one exception to this is 0.0.0.0; on many systems, attempting to
    // navigate to this IP actually navigates to localhost.  To support this
    // case, when the converted IP is 0.0.0.0, we go ahead and run the "did the
    // user actually type four components" test in the conditional below, so
    // that we'll allow explicit attempts to navigate to "0.0.0.0".  If the
    // input was anything else (e.g. "0"), we'll fall through to returning QUERY
    // afterwards.
    if ((host_info.address[0] != 0) ||
        ((host_info.address[1] == 0) && (host_info.address[2] == 0) &&
         (host_info.address[3] == 0))) {
      // This is theoretically a navigable IP.  We have four cases.  The first
      // three are:
      // * If the user typed four distinct components, this is an IP for sure.
      // * If the user typed two or three components, this is almost certainly a
      //   query, especially for two components (as in "13.5/7.25"), but we'll
      //   allow navigation for an explicit scheme or trailing slash below.
      // * If the user typed one component, this is likely a query, but could be
      //   a non-dotted-quad version of an IP address.
      // Unfortunately, since we called CanonicalizeHost() on the
      // already-canonicalized host, all of these cases will have been changed
      // to have four components (e.g. 13.2 -> 13.0.0.2), so we have to call
      // CanonicalizeHost() again, this time on the original input, so that we
      // can get the correct number of IP components.
      //
      // The fourth case is that the user typed something ambiguous like ".1.2"
      // that fixup converted to an IP address ("1.0.0.2").  In this case the
      // call to CanonicalizeHost() will return NEUTRAL here.  Since it's not
      // clear what the user intended, we fall back to our other heuristics.
      net::CanonicalizeHost(base::UTF16ToUTF8(original_host), &host_info);
      if ((host_info.family == url::CanonHostInfo::IPV4) &&
          (host_info.num_ipv4_components == 4))
        return metrics::OmniboxInputType::URL;
    }

    // By this point, if we have an "IP" with first octet zero, we know it
    // wasn't "0.0.0.0", so mark it as non-navigable.
    if (host_info.address[0] == 0)
      return metrics::OmniboxInputType::QUERY;
  }

  // Now that we've ruled out all schemes other than http or https and done a
  // little more sanity checking, the presence of a scheme means this is likely
  // a URL.
  if (parts->scheme.is_nonempty())
    return metrics::OmniboxInputType::URL;

  // Check to see if the username is set and, if so, whether it contains a
  // space.  Usernames usually do not contain a space.  If a username contains
  // a space, that's likely an indication of incorrectly parsing of the input.
  const bool username_has_space =
      parts->username.is_nonempty() &&
      (text.substr(parts->username.begin, parts->username.len)
           .find_first_of(base::kWhitespaceUTF16) != base::string16::npos);

  // Generally, trailing slashes force the input to be treated as a URL.
  // However, if the username has a space, this may be input like
  // "dep missing: @test/", which should not be parsed as a URL (with the
  // username "dep missing: ").
  if (parts->path.is_nonempty() && !username_has_space) {
    base::char16 c = text[parts->path.end() - 1];
    if ((c == '\\') || (c == '/'))
      return metrics::OmniboxInputType::URL;
  }

  // Handle the cases we detected in the IPv4 code above as "almost certainly a
  // query" now that we know the user hasn't tried to force navigation via a
  // scheme/trailing slash.
  if ((host_info.family == url::CanonHostInfo::IPV4) &&
      (host_info.num_ipv4_components > 1))
    return metrics::OmniboxInputType::QUERY;

  // The URL did not have an explicit scheme and has an unusual-looking
  // username (with a space).  It's not likely to be a URL.
  if (username_has_space)
    return metrics::OmniboxInputType::UNKNOWN;

  // If there is more than one recognized non-host component, this is likely to
  // be a URL, even if the TLD is unknown (in which case this is likely an
  // intranet URL).
  if (NumNonHostComponents(*parts) > 1)
    return metrics::OmniboxInputType::URL;

  // If we reach here with a username, our input looks something like
  // "user@host".  Unless there is a desired TLD, we think this is more likely
  // an email address than an HTTP auth attempt, so we search by default.  (When
  // there _is_ a desired TLD, the user hit ctrl-enter, and we assume that
  // implies an attempted navigation.)
  if (canonicalized_url->has_username() && desired_tld.empty())
    return metrics::OmniboxInputType::UNKNOWN;

  // If the host has a known TLD or a port, it's probably a URL. Just localhost
  // is considered a valid host name due to https://tools.ietf.org/html/rfc6761.
  if (has_known_tld || canonicalized_url->DomainIs("localhost") ||
      canonicalized_url->has_port())
    return metrics::OmniboxInputType::URL;

  // The .example and .test TLDs are special-cased as known TLDs due to
  // https://tools.ietf.org/html/rfc6761. Unlike localhost, these are not valid
  // host names, so they must have at least one subdomain to be a URL.
  for (const base::StringPiece domain : {"example", "test"}) {
    // The +1 accounts for a possible trailing period.
    if (canonicalized_url->DomainIs(domain) &&
        (canonicalized_url->host().length() > (domain.length() + 1)))
      return metrics::OmniboxInputType::URL;
  }

  // No scheme, username, port, and no known TLD on the host.
  // This could be:
  // * A single word "foo"; possibly an intranet site, but more likely a search.
  //   This is ideally an UNKNOWN, and we can let the Alternate Nav URL code
  //   catch our mistakes.
  // * A URL with a valid TLD we don't know about yet.  If e.g. a registrar adds
  //   "xxx" as a TLD, then until we add it to our data file, Chrome won't know
  //   "foo.xxx" is a real URL.  So ideally this is a URL, but we can't really
  //   distinguish this case from:
  // * A "URL-like" string that's not really a URL (like
  //   "browser.tabs.closeButtons" or "java.awt.event.*").  This is ideally a
  //   QUERY.  Since this is indistinguishable from the case above, and this
  //   case is much more likely, claim these are UNKNOWN, which should default
  //   to the right thing and let users correct us on a case-by-case basis.
  return metrics::OmniboxInputType::UNKNOWN;
}

// static
void AutocompleteInput::ParseForEmphasizeComponents(
    const base::string16& text,
    const AutocompleteSchemeClassifier& scheme_classifier,
    url::Component* scheme,
    url::Component* host) {
  url::Parsed parts;
  base::string16 scheme_str;
  Parse(text, std::string(), scheme_classifier, &parts, &scheme_str, nullptr);

  *scheme = parts.scheme;
  *host = parts.host;

  int after_scheme_and_colon = parts.scheme.end() + 1;
  // For the view-source and blob schemes, we should emphasize the host of the
  // URL qualified by the view-source or blob prefix.
  if ((base::LowerCaseEqualsASCII(scheme_str, kViewSourceScheme) ||
       base::LowerCaseEqualsASCII(scheme_str, url::kBlobScheme)) &&
      (static_cast<int>(text.length()) > after_scheme_and_colon)) {
    // Obtain the URL prefixed by view-source or blob and parse it.
    base::string16 real_url(text.substr(after_scheme_and_colon));
    url::Parsed real_parts;
    AutocompleteInput::Parse(real_url, std::string(), scheme_classifier,
                             &real_parts, nullptr, nullptr);
    if (real_parts.scheme.is_nonempty() || real_parts.host.is_nonempty()) {
      if (real_parts.scheme.is_nonempty()) {
        *scheme = url::Component(
            after_scheme_and_colon + real_parts.scheme.begin,
            real_parts.scheme.len);
      } else {
        scheme->reset();
      }
      if (real_parts.host.is_nonempty()) {
        *host = url::Component(after_scheme_and_colon + real_parts.host.begin,
                               real_parts.host.len);
      } else {
        host->reset();
      }
    }
  } else if (base::LowerCaseEqualsASCII(scheme_str, url::kFileSystemScheme) &&
             parts.inner_parsed() && parts.inner_parsed()->scheme.is_valid()) {
    *host = parts.inner_parsed()->host;
  }
}

// static
base::string16 AutocompleteInput::FormattedStringWithEquivalentMeaning(
    const GURL& url,
    const base::string16& formatted_url,
    const AutocompleteSchemeClassifier& scheme_classifier,
    size_t* offset) {
  if (!url_formatter::CanStripTrailingSlash(url))
    return formatted_url;
  const base::string16 url_with_path(formatted_url + base::char16('/'));
  if (AutocompleteInput::Parse(formatted_url, std::string(), scheme_classifier,
                               nullptr, nullptr, nullptr) ==
      AutocompleteInput::Parse(url_with_path, std::string(), scheme_classifier,
                               nullptr, nullptr, nullptr)) {
    return formatted_url;
  }
  // If offset is past the addition, shift it.
  if (offset && *offset == formatted_url.size())
    ++(*offset);
  return url_with_path;
}

// static
int AutocompleteInput::NumNonHostComponents(const url::Parsed& parts) {
  int num_nonhost_components = 0;
  if (parts.scheme.is_nonempty())
    ++num_nonhost_components;
  if (parts.username.is_nonempty())
    ++num_nonhost_components;
  if (parts.password.is_nonempty())
    ++num_nonhost_components;
  if (parts.port.is_nonempty())
    ++num_nonhost_components;
  if (parts.path.is_nonempty())
    ++num_nonhost_components;
  if (parts.query.is_nonempty())
    ++num_nonhost_components;
  if (parts.ref.is_nonempty())
    ++num_nonhost_components;
  return num_nonhost_components;
}

// static
bool AutocompleteInput::HasHTTPScheme(const base::string16& input) {
  std::string utf8_input(base::UTF16ToUTF8(input));
  url::Component scheme;
  if (url::FindAndCompareScheme(utf8_input, kViewSourceScheme, &scheme)) {
    utf8_input.erase(0, scheme.end() + 1);
  }
  return url::FindAndCompareScheme(utf8_input, url::kHttpScheme, nullptr);
}

void AutocompleteInput::UpdateText(const base::string16& text,
                                   size_t cursor_position,
                                   const url::Parsed& parts) {
  DCHECK(cursor_position <= text.length() ||
         cursor_position == base::string16::npos)
      << "Text: '" << text << "', cp: " << cursor_position;
  text_ = text;
  cursor_position_ = cursor_position;
  parts_ = parts;
}

void AutocompleteInput::Clear() {
  text_.clear();
  cursor_position_ = base::string16::npos;
  current_url_ = GURL();
  current_title_.clear();
  current_page_classification_ = metrics::OmniboxEventProto::INVALID_SPEC;
  type_ = metrics::OmniboxInputType::EMPTY;
  parts_ = url::Parsed();
  scheme_.clear();
  canonicalized_url_ = GURL();
  prevent_inline_autocomplete_ = false;
  prefer_keyword_ = false;
  allow_exact_keyword_match_ = false;
  want_asynchronous_matches_ = true;
  from_omnibox_focus_ = false;
  terms_prefixed_by_http_or_https_.clear();
}

size_t AutocompleteInput::EstimateMemoryUsage() const {
  size_t res = 0;

  res += base::trace_event::EstimateMemoryUsage(text_);
  res += base::trace_event::EstimateMemoryUsage(current_url_);
  res += base::trace_event::EstimateMemoryUsage(current_title_);
  res += base::trace_event::EstimateMemoryUsage(scheme_);
  res += base::trace_event::EstimateMemoryUsage(canonicalized_url_);
  res += base::trace_event::EstimateMemoryUsage(desired_tld_);
  res +=
      base::trace_event::EstimateMemoryUsage(terms_prefixed_by_http_or_https_);

  return res;
}
