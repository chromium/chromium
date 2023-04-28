// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_URL_IDENTITY_H_
#define CHROME_BROWSER_UI_URL_IDENTITY_H_

#include <string>

#include "base/containers/enum_set.h"

class Profile;
class GURL;

// UrlIdentity is the identity of a URL suitable for displaying to the user.
// UrlIdentity has 2 main properties:
// - name: a string which users can use to identify the url.
// - type: the type of subject. (i.e. site, extension, file, etc)
struct UrlIdentity {
  enum class Type {
    kMinValue = 0,
    // Default type are human-Identifiable URLs.
    // i.e. DNS-based sites
    // Their identity is a variant of the URL.
    // Any URL not captured by other types will be handled as default.
    kDefault = kMinValue,
    kChromeExtension,
    kIsolatedWebApp,
    kFile,
    kMaxValue = kFile
  };

  using TypeSet = base::EnumSet<Type, Type::kMinValue, Type::kMaxValue>;

  // Formatting options for default type.
  enum class DefaultFormatOptions {
    kMinValue,
    // Returns the `GURL::spec()`.
    kRawSpec = kMinValue,
    // Omit cryptographic scheme. (i.e. https and wss)
    kOmitCryptographicScheme,
    // Formats a URL in a concise and human-friendly way, omits the HTTP/HTTPS
    // scheme, the username and password, the path and removes trivial
    // subdomains. See
    // `url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains()`.
    kOmitSchemePathAndTrivialSubdomains,
    // Returns the hostname in unicode. Returns ASCII hostname if not IDN or
    // invalid.
    kHostname,
    kMaxValue = kHostname
  };

  struct FormatOptions {
    // Holds options for formatting default type.
    base::EnumSet<DefaultFormatOptions,
                  DefaultFormatOptions::kMinValue,
                  DefaultFormatOptions::kMaxValue>
        default_options;
  };

  // Creates a |UrlIdentity| from the |url|, using |options| as customization
  // options. A non-null |profile| is required to handle some subject types.
  // Caller is responsible for explicitly allowing a type to be handled by the
  // API by adding the type into |allowed_types|. If |CreateFromUrl| encounters
  // a type not defined in |allowed_types|:
  // - Debug build:
  //   - Crash w/ error message for debug builds.
  // - Prod build:
  //   - If the type is non-default, a error will be logged. The call will be
  //   redirected to be handled as default type.
  //   - If |kDefault| is not allowed, any default type call will crash.
  //   Non-default calls redirected to be handled as default will also crash.
  static UrlIdentity CreateFromUrl(Profile* profile,
                                   const GURL& url,
                                   const TypeSet& allowed_types,
                                   const FormatOptions& options);

  Type type;
  std::u16string name;
};

#endif  // CHROME_BROWSER_UI_URL_IDENTITY_H_
