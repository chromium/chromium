// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/url_fixer.h"

#include <stddef.h>

#include <algorithm>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/escape.h"
#include "net/base/filename_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_file.h"
#include "url/url_util.h"

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
#include "base/path_service.h"
#endif

namespace url_formatter {

const char* home_directory_override = nullptr;

namespace {

// Hardcode these constants to avoid dependences on //chrome and //content.
const char kChromeUIScheme[] = "chrome";
const char kDevToolsScheme[] = "devtools";
const char kDevToolsFallbackScheme[] = "chrome-devtools";
const char kChromeUIDefaultHost[] = "version";
const char kViewSourceScheme[] = "view-source";

// TODO(estade): Remove these ugly, ugly functions. They are only used in
// SegmentURL. A url::Parsed object keeps track of a bunch of indices into
// a url string, and these need to be updated when the URL is converted from
// UTF8 to UTF16. Instead of this after-the-fact adjustment, we should parse it
// in the correct string format to begin with.
url::Component UTF8ComponentToUTF16Component(
    const std::string& text_utf8,
    const url::Component& component_utf8) {
  if (component_utf8.len == -1)
    return url::Component();

  std::string before_component_string =
      text_utf8.substr(0, component_utf8.begin);
  std::string component_string =
      text_utf8.substr(component_utf8.begin, component_utf8.len);
  base::string16 before_component_string_16 =
      base::UTF8ToUTF16(before_component_string);
  base::string16 component_string_16 = base::UTF8ToUTF16(component_string);
  url::Component component_16(before_component_string_16.length(),
                              component_string_16.length());
  return component_16;
}

void UTF8PartsToUTF16Parts(const std::string& text_utf8,
                           const url::Parsed& parts_utf8,
                           url::Parsed* parts) {
  if (base::IsStringASCII(text_utf8)) {
    *parts = parts_utf8;
    return;
  }

  parts->scheme = UTF8ComponentToUTF16Component(text_utf8, parts_utf8.scheme);
  parts->username =
      UTF8ComponentToUTF16Component(text_utf8, parts_utf8.username);
  parts->password =
      UTF8ComponentToUTF16Component(text_utf8, parts_utf8.password);
  parts->host = UTF8ComponentToUTF16Component(text_utf8, parts_utf8.host);
  parts->port = UTF8ComponentToUTF16Component(text_utf8, parts_utf8.port);
  parts->path = UTF8ComponentToUTF16Component(text_utf8, parts_utf8.path);
  parts->query = UTF8ComponentToUTF16Component(text_utf8, parts_utf8.query);
  parts->ref = UTF8ComponentToUTF16Component(text_utf8, parts_utf8.ref);
}

base::TrimPositions TrimWhitespaceUTF8(const std::string& input,
                                       base::TrimPositions positions,
                                       std::string* output) {
  // This implementation is not so fast since it converts the text encoding
  // twice. Please feel free to file a bug if this function hurts the
  // performance of Chrome.
  DCHECK(base::IsStringUTF8(input));
  base::string16 input16 = base::UTF8ToUTF16(input);
  base::string16 output16;
  base::TrimPositions result =
      base::TrimWhitespace(input16, positions, &output16);
  *output = base::UTF16ToUTF8(output16);
  return result;
}

// does some basic fixes for input that we want to test for file-ness
void PrepareStringForFileOps(const base::FilePath& text,
                             base::FilePath::StringType* output) {
#if defined(OS_WIN)
  base::TrimWhitespace(text.value(), base::TRIM_ALL, output);
  replace(output->begin(), output->end(), '/', '\\');
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  TrimWhitespaceUTF8(text.value(), base::TRIM_ALL, output);
#else
#error Unsupported platform
#endif
}

// Tries to create a full path from |text|.  If the result is valid and the
// file exists, returns true and sets |full_path| to the result.  Otherwise,
// returns false and leaves |full_path| unchanged.
bool ValidPathForFile(const base::FilePath::StringType& text,
                      base::FilePath* full_path) {
  base::FilePath file_path = base::MakeAbsoluteFilePath(base::FilePath(text));
  if (file_path.empty())
    return false;

  if (!base::PathExists(file_path))
    return false;

  *full_path = file_path;
  return true;
}

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
// Given a path that starts with ~, return a path that starts with an
// expanded-out /user/foobar directory.
std::string FixupHomedir(const std::string& text) {
  DCHECK(text.length() > 0 && text[0] == '~');

  if (text.length() == 1 || text[1] == '/') {
    base::FilePath file_path;
    if (home_directory_override)
      file_path = base::FilePath(home_directory_override);
    else
      base::PathService::Get(base::DIR_HOME, &file_path);

    // We'll probably break elsewhere if $HOME is undefined, but check here
    // just in case.
    if (file_path.value().empty())
      return text;
    // Append requires to be a relative path, so we have to cut all preceeding
    // '/' characters.
    size_t i = 1;
    while (i < text.length() && text[i] == '/')
      ++i;
    return file_path.Append(text.substr(i)).value();
  }

// Otherwise, this is a path like ~foobar/baz, where we must expand to
// user foobar's home directory.  Officially, we should use getpwent(),
// but that is a nasty blocking call.

#if defined(OS_MACOSX)
  static const char kHome[] = "/Users/";
#else
  static const char kHome[] = "/home/";
#endif
  return kHome + text.substr(1);
}
#endif

// Tries to create a file: URL from |text| if it looks like a filename, even if
// it doesn't resolve as a valid path or to an existing file.  Returns a
// (possibly invalid) file: URL in |fixed_up_url| for input beginning
// with a drive specifier or "\\".  Returns the unchanged input in other cases
// (including file: URLs: these don't look like filenames).
std::string FixupPath(const std::string& text) {
  DCHECK(!text.empty());

  base::FilePath::StringType filename;
#if defined(OS_WIN)
  base::FilePath input_path(base::UTF8ToWide(text));
  PrepareStringForFileOps(input_path, &filename);

  // Fixup Windows-style drive letters, where "C:" gets rewritten to "C|".
  if (filename.length() > 1 && filename[1] == '|')
    filename[1] = ':';
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  base::FilePath input_path(text);
  PrepareStringForFileOps(input_path, &filename);
  if (filename.length() > 0 && filename[0] == '~')
    filename = FixupHomedir(filename);
#endif

  // Here, we know the input looks like a file.
  GURL file_url = net::FilePathToFileURL(base::FilePath(filename));
  if (file_url.is_valid()) {
    return base::UTF16ToUTF8(url_formatter::FormatUrl(
        file_url, url_formatter::kFormatUrlOmitUsernamePassword,
        net::UnescapeRule::NORMAL, nullptr, nullptr, nullptr));
  }

  // Invalid file URL, just return the input.
  return text;
}

// Checks |domain| to see if a valid TLD is already present.  If not, appends
// |desired_tld| to the domain, and prepends "www." unless it's already present.
void AddDesiredTLD(const std::string& desired_tld, std::string* domain) {
  if (desired_tld.empty() || domain->empty())
    return;

  // Abort if we already have a known TLD. In the case of an invalid host,
  // HostHasRegistryControlledDomain will return false and we will try to
  // append a TLD (which may make it valid). For example, "999999999999" is
  // detected as a broken IP address and marked invalid, but attaching ".com"
  // makes it legal.  We disallow unknown registries here so users can input
  // "mail.yahoo" and hit ctrl-enter to get "www.mail.yahoo.com".
  if (net::registry_controlled_domains::HostHasRegistryControlledDomain(
          *domain, net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES))
    return;

  // Add the suffix at the end of the domain.
  const size_t domain_length(domain->length());
  DCHECK_GT(domain_length, 0U);
  DCHECK_NE(desired_tld[0], '.');
  if ((*domain)[domain_length - 1] != '.')
    domain->push_back('.');
  domain->append(desired_tld);

  // Now, if the domain begins with "www.", stop.
  const std::string prefix("www.");
  if (domain->compare(0, prefix.length(), prefix) != 0) {
    // Otherwise, add www. to the beginning of the URL.
    domain->insert(0, prefix);
  }
}

inline void FixupUsername(const std::string& text,
                          const url::Component& part,
                          std::string* url) {
  if (!part.is_valid())
    return;

  // We don't fix up the username at the moment.
  url->append(text, part.begin, part.len);
  // Do not append the trailing '@' because we might need to include the user's
  // password.  FixupURL itself will append the '@' for us.
}

inline void FixupPassword(const std::string& text,
                          const url::Component& part,
                          std::string* url) {
  if (!part.is_valid())
    return;

  // We don't fix up the password at the moment.
  url->append(":");
  url->append(text, part.begin, part.len);
}

void FixupHost(const std::string& text,
               const url::Component& part,
               bool has_scheme,
               const std::string& desired_tld,
               std::string* url) {
  if (!part.is_valid())
    return;

  // Make domain valid.
  // Strip all leading dots and all but one trailing dot, unless the user only
  // typed dots, in which case their input is totally invalid and we should just
  // leave it unchanged.
  std::string domain(text, part.begin, part.len);
  const size_t first_nondot(domain.find_first_not_of('.'));
  if (first_nondot != std::string::npos) {
    domain.erase(0, first_nondot);
    size_t last_nondot(domain.find_last_not_of('.'));
    DCHECK(last_nondot != std::string::npos);
    last_nondot += 2;  // Point at second period in ending string
    if (last_nondot < domain.length())
      domain.erase(last_nondot);
  }

  // Add any user-specified TLD, if applicable.
  AddDesiredTLD(desired_tld, &domain);

  url->append(domain);
}

void FixupPort(const std::string& text,
               const url::Component& part,
               std::string* url) {
  if (!part.is_valid())
    return;

  // We don't fix up the port at the moment.
  url->append(":");
  url->append(text, part.begin, part.len);
}

inline void FixupPath(const std::string& text,
                      const url::Component& part,
                      std::string* url) {
  if (!part.is_valid() || part.len == 0) {
    // We should always have a path.
    url->append("/");
    return;
  }

  // Append the path as is.
  url->append(text, part.begin, part.len);
}

inline void FixupQuery(const std::string& text,
                       const url::Component& part,
                       std::string* url) {
  if (!part.is_valid())
    return;

  // We don't fix up the query at the moment.
  url->append("?");
  url->append(text, part.begin, part.len);
}

inline void FixupRef(const std::string& text,
                     const url::Component& part,
                     std::string* url) {
  if (!part.is_valid())
    return;

  // We don't fix up the ref at the moment.
  url->append("#");
  url->append(text, part.begin, part.len);
}

bool HasPort(const std::string& original_text,
             const url::Component& scheme_component) {
  // Find the range between the ":" and the "/".
  size_t port_start = scheme_component.end() + 1;
  size_t port_end = port_start;
  while ((port_end < original_text.length()) &&
         !url::IsAuthorityTerminator(original_text[port_end]))
    ++port_end;
  if (port_end == port_start)
    return false;

  // Scan the range to see if it is entirely digits.
  for (size_t i = port_start; i < port_end; ++i) {
    if (!base::IsAsciiDigit(original_text[i]))
      return false;
  }

  return true;
}

// Try to extract a valid scheme from the beginning of |text|.
// If successful, set |scheme_component| to the text range where the scheme
// was located, and fill |canon_scheme| with its canonicalized form.
// Otherwise, return false and leave the outputs in an indeterminate state.
bool GetValidScheme(const std::string& text,
                    url::Component* scheme_component,
                    std::string* canon_scheme) {
  canon_scheme->clear();

  // Locate everything up to (but not including) the first ':'
  if (!url::ExtractScheme(text.data(), static_cast<int>(text.length()),
                          scheme_component)) {
    return false;
  }

  // Make sure the scheme contains only valid characters, and convert
  // to lowercase.  This also catches IPv6 literals like [::1], because
  // brackets are not in the whitelist.
  url::StdStringCanonOutput canon_scheme_output(canon_scheme);
  url::Component canon_scheme_component;
  if (!url::CanonicalizeScheme(text.data(), *scheme_component,
                               &canon_scheme_output, &canon_scheme_component)) {
    return false;
  }

  // Strip the ':', and any trailing buffer space.
  DCHECK_EQ(0, canon_scheme_component.begin);
  canon_scheme->erase(canon_scheme_component.len);

  // We need to fix up the segmentation for "www.example.com:/".  For this
  // case, we guess that schemes with a "." are not actually schemes.
  if (canon_scheme->find('.') != std::string::npos)
    return false;

  // We need to fix up the segmentation for "www:123/".  For this case, we
  // will add an HTTP scheme later and make the URL parser happy.
  // TODO(pkasting): Maybe we should try to use GURL's parser for this?
  if (HasPort(text, *scheme_component))
    return false;

  // Everything checks out.
  return true;
}

// Performs the work for url_formatter::SegmentURL. |text| may be modified on
// output on success: a semicolon following a valid scheme is replaced with a
// colon.
std::string SegmentURLInternal(std::string* text, url::Parsed* parts) {
  // Initialize the result.
  *parts = url::Parsed();

  std::string trimmed;
  TrimWhitespaceUTF8(*text, base::TRIM_ALL, &trimmed);
  if (trimmed.empty())
    return std::string();  // Nothing to segment.

  std::string scheme;
#if defined(OS_WIN)
  int trimmed_length = static_cast<int>(trimmed.length());
  if (url::DoesBeginWindowsDriveSpec(trimmed.data(), 0, trimmed_length) ||
      url::DoesBeginUNCPath(trimmed.data(), 0, trimmed_length, true))
    scheme = url::kFileScheme;
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  if (base::FilePath::IsSeparator(trimmed.data()[0]) ||
      trimmed.data()[0] == '~')
    scheme = url::kFileScheme;
#endif

  // Otherwise, we need to look at things carefully.
  if (scheme.empty() && !GetValidScheme(*text, &parts->scheme, &scheme)) {
    // Try again if there is a ';' in the text. If changing it to a ':' results
    // in a standard scheme, "about", "chrome" or "file" scheme being found,
    // continue processing with the modified text.
    bool found_scheme = false;
    size_t semicolon = text->find(';');
    if (semicolon != 0 && semicolon != std::string::npos) {
      (*text)[semicolon] = ':';
      if (GetValidScheme(*text, &parts->scheme, &scheme) &&
          (url::IsStandard(
               scheme.c_str(),
               url::Component(0, static_cast<int>(scheme.length()))) ||
           scheme == url::kAboutScheme || scheme == kChromeUIScheme ||
           scheme == url::kFileScheme))
        found_scheme = true;
      else
        (*text)[semicolon] = ';';
    }
    if (!found_scheme) {
      // Couldn't determine the scheme, so just default to http.
      parts->scheme.reset();
      scheme = url::kHttpScheme;
    }
  }

  // Proceed with about, chrome, and devtools schemes,
  // but not file or nonstandard schemes.
  if ((scheme != url::kAboutScheme) && (scheme != kChromeUIScheme) &&
      (scheme != kDevToolsScheme) && (scheme != kDevToolsFallbackScheme) &&
      !url::IsStandard(scheme.c_str(),
                       url::Component(0, static_cast<int>(scheme.length())))) {
    return scheme;
  }

  int text_length = static_cast<int>(text->length());
  if (scheme == url::kFileScheme) {
    url::ParseFileURL(text->data(), text_length, parts);
    return scheme;
  }

  if (scheme == url::kFileSystemScheme) {
    // Have the GURL parser do the heavy lifting for us.
    url::ParseFileSystemURL(text->data(), text_length, parts);
    return scheme;
  }

  if (scheme == kDevToolsFallbackScheme) {
    // Have the GURL parser do the heavy lifting for us.
    url::ParseStandardURL(text->data(), text_length, parts);
    // Now replace the fallback scheme alias with the real one
    parts->scheme.reset();
    return kDevToolsScheme;
  }

  if (parts->scheme.is_valid()) {
    // Have the GURL parser do the heavy lifting for us.
    url::ParseStandardURL(text->data(), text_length, parts);
    return scheme;
  }

  // We need to add a scheme in order for ParseStandardURL to be happy.
  // Find the first non-whitespace character.
  std::string::iterator first_nonwhite = text->begin();
  while ((first_nonwhite != text->end()) &&
         base::IsUnicodeWhitespace(*first_nonwhite))
    ++first_nonwhite;

  // Construct the text to parse by inserting the scheme.
  std::string inserted_text(scheme);
  // Assume a leading colon was meant to be a scheme separator (which GURL will
  // fix up for us into the full "://").  Otherwise add the separator ourselves.
  if (first_nonwhite == text->end() || *first_nonwhite != ':')
    inserted_text.append(url::kStandardSchemeSeparator);
  std::string text_to_parse(text->begin(), first_nonwhite);
  text_to_parse.append(inserted_text);
  text_to_parse.append(first_nonwhite, text->end());

  // Have the GURL parser do the heavy lifting for us.
  url::ParseStandardURL(text_to_parse.data(),
                        static_cast<int>(text_to_parse.length()), parts);

  // Offset the results of the parse to match the original text.
  const int offset = -static_cast<int>(inserted_text.length());
  OffsetComponent(offset, &parts->scheme);
  OffsetComponent(offset, &parts->username);
  OffsetComponent(offset, &parts->password);
  OffsetComponent(offset, &parts->host);
  OffsetComponent(offset, &parts->port);
  OffsetComponent(offset, &parts->path);
  OffsetComponent(offset, &parts->query);
  OffsetComponent(offset, &parts->ref);

  return scheme;
}

}  // namespace

std::string SegmentURL(const std::string& text, url::Parsed* parts) {
  std::string mutable_text(text);
  return SegmentURLInternal(&mutable_text, parts);
}

base::string16 SegmentURL(const base::string16& text, url::Parsed* parts) {
  std::string text_utf8 = base::UTF16ToUTF8(text);
  url::Parsed parts_utf8;
  std::string scheme_utf8 = SegmentURL(text_utf8, &parts_utf8);
  UTF8PartsToUTF16Parts(text_utf8, parts_utf8, parts);
  return base::UTF8ToUTF16(scheme_utf8);
}

GURL FixupURL(const std::string& text, const std::string& desired_tld) {
  std::string trimmed;
  TrimWhitespaceUTF8(text, base::TRIM_ALL, &trimmed);
  if (trimmed.empty())
    return GURL();  // Nothing here.

  // Segment the URL.
  url::Parsed parts;
  std::string scheme(SegmentURLInternal(&trimmed, &parts));

  // For view-source: URLs, we strip "view-source:", do fixup, and stick it back
  // on.  This allows us to handle things like "view-source:google.com".
  if (scheme == kViewSourceScheme) {
    // Reject "view-source:view-source:..." to avoid deep recursion.
    std::string view_source(kViewSourceScheme + std::string(":"));
    if (!base::StartsWith(text, view_source + view_source,
                          base::CompareCase::INSENSITIVE_ASCII)) {
      return GURL(kViewSourceScheme + std::string(":") +
                  FixupURL(trimmed.substr(scheme.length() + 1), desired_tld)
                      .possibly_invalid_spec());
    }
  }

  // We handle the file scheme separately.
  if (scheme == url::kFileScheme)
    return GURL(parts.scheme.is_valid() ? text : FixupPath(text));

  // We handle the filesystem scheme separately.
  if (scheme == url::kFileSystemScheme) {
    if (parts.inner_parsed() && parts.inner_parsed()->scheme.is_valid())
      return GURL(text);
    return GURL();
  }

  // 'about:blank' and 'about:srcdoc' are special-cased in various places in the
  // code and shouldn't use the chrome: scheme.
  if (base::LowerCaseEqualsASCII(scheme, url::kAboutScheme)) {
    GURL about_url(base::ToLowerASCII(trimmed));
    if (about_url.IsAboutBlank() || about_url.IsAboutSrcdoc())
      return about_url;
  }

  // Parse and rebuild about: and chrome: URLs.
  bool chrome_url =
      (scheme == url::kAboutScheme) || (scheme == kChromeUIScheme);

  // Parse and rebuild devtools: and the fallback chrome-devtools: URLs.
  bool devtools_url =
      (scheme == kDevToolsScheme) || (scheme == kDevToolsFallbackScheme);

  // For some schemes whose layouts we understand, we rebuild it.
  if (chrome_url || devtools_url ||
      url::IsStandard(scheme.c_str(),
                      url::Component(0, static_cast<int>(scheme.length())))) {
    // Replace the about: scheme with the chrome: scheme, or
    // chrome-devtoools: scheme with the devtools: scheme.
    std::string url(chrome_url ? kChromeUIScheme
                               : devtools_url ? kDevToolsScheme : scheme);
    url.append(url::kStandardSchemeSeparator);

    // We need to check whether the |username| is valid because it is our
    // responsibility to append the '@' to delineate the user information from
    // the host portion of the URL.
    if (parts.username.is_valid()) {
      FixupUsername(trimmed, parts.username, &url);
      FixupPassword(trimmed, parts.password, &url);
      url.append("@");
    }

    FixupHost(trimmed, parts.host, parts.scheme.is_valid(), desired_tld, &url);
    if (chrome_url && !parts.host.is_valid())
      url.append(kChromeUIDefaultHost);
    FixupPort(trimmed, parts.port, &url);
    FixupPath(trimmed, parts.path, &url);
    FixupQuery(trimmed, parts.query, &url);
    FixupRef(trimmed, parts.ref, &url);

    return GURL(url);
  }

  // In the worst-case, we insert a scheme if the URL lacks one.
  if (!parts.scheme.is_valid()) {
    std::string fixed_scheme(scheme);
    fixed_scheme.append(url::kStandardSchemeSeparator);
    trimmed.insert(0, fixed_scheme);
  }

  return GURL(trimmed);
}

// The rules are different here than for regular fixup, since we need to handle
// input like "hello.html" and know to look in the current directory.  Regular
// fixup will look for cues that it is actually a file path before trying to
// figure out what file it is.  If our logic doesn't work, we will fall back on
// regular fixup.
GURL FixupRelativeFile(const base::FilePath& base_dir,
                       const base::FilePath& text) {
  base::FilePath old_cur_directory;
  if (!base_dir.empty()) {
    // Save the old current directory before we move to the new one.
    base::GetCurrentDirectory(&old_cur_directory);
    base::SetCurrentDirectory(base_dir);
  }

  // Allow funny input with extra whitespace and the wrong kind of slashes.
  base::FilePath::StringType trimmed;
  PrepareStringForFileOps(text, &trimmed);

  bool is_file = true;
  // Avoid recognizing definite non-file URLs as file paths.
  GURL gurl(trimmed);
  if (gurl.is_valid() && gurl.IsStandard())
    is_file = false;
  base::FilePath full_path;
  if (is_file && !ValidPathForFile(trimmed, &full_path)) {
// Not a path as entered, try unescaping it in case the user has
// escaped things. We need to go through 8-bit since the escaped values
// only represent 8-bit values.
#if defined(OS_WIN)
    std::wstring unescaped = base::UTF8ToWide(net::UnescapeURLComponent(
        base::WideToUTF8(trimmed),
        net::UnescapeRule::SPACES | net::UnescapeRule::PATH_SEPARATORS |
            net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS));
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
    std::string unescaped = net::UnescapeURLComponent(
        trimmed,
        net::UnescapeRule::SPACES | net::UnescapeRule::PATH_SEPARATORS |
            net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
#endif

    if (!ValidPathForFile(unescaped, &full_path))
      is_file = false;
  }

  // Put back the current directory if we saved it.
  if (!base_dir.empty())
    base::SetCurrentDirectory(old_cur_directory);

  if (is_file) {
    GURL file_url = net::FilePathToFileURL(full_path);
    if (file_url.is_valid())
      return GURL(base::UTF16ToUTF8(url_formatter::FormatUrl(
          file_url, url_formatter::kFormatUrlOmitUsernamePassword,
          net::UnescapeRule::NORMAL, nullptr, nullptr, nullptr)));
    // Invalid files fall through to regular processing.
  }

// Fall back on regular fixup for this input.
#if defined(OS_WIN)
  std::string text_utf8 = base::WideToUTF8(text.value());
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  std::string text_utf8 = text.value();
#endif
  return FixupURL(text_utf8, std::string());
}

void OffsetComponent(int offset, url::Component* part) {
  DCHECK(part);

  if (part->is_valid()) {
    // Offset the location of this component.
    part->begin += offset;

    // This part might not have existed in the original text.
    if (part->begin < 0)
      part->reset();
  }
}

bool IsEquivalentScheme(const std::string& scheme1,
                        const std::string& scheme2) {
  return scheme1 == scheme2 ||
         (scheme1 == url::kAboutScheme && scheme2 == kChromeUIScheme) ||
         (scheme1 == kChromeUIScheme && scheme2 == url::kAboutScheme) ||
         (scheme1 == kDevToolsScheme && scheme2 == kDevToolsFallbackScheme) ||
         (scheme1 == kDevToolsFallbackScheme && scheme2 == kDevToolsScheme);
}

}  // namespace url_formatter
