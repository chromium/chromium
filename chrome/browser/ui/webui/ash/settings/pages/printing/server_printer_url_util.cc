// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/printing/server_printer_url_util.h"

#include "url/gurl.h"

namespace {

// Returns an updated |gurl| with the specified components from the params. If
// |scheme| is not empty, returns an updated GURL with the specified scheme. If
// |replace_port| is true, returns an updated GURL with 631 as the port. 631 is
// the default port for IPP.
GURL UpdateServerPrinterGURL(const GURL& gurl,
                             const std::string& scheme,
                             bool replace_ipp_port) {
  GURL::Replacements replacement;
  if (!scheme.empty()) {
    replacement.SetSchemeStr(scheme);
  }
  if (replace_ipp_port) {
    replacement.SetPortStr("631");
  }
  return gurl.ReplaceComponents(replacement);
}

}  // namespace

namespace ash::settings {

bool HasValidServerPrinterScheme(const GURL& gurl) {
  return gurl.SchemeIsHTTPOrHTTPS() || gurl.SchemeIs("ipp") ||
         gurl.SchemeIs("ipps");
}

std::optional<GURL> GenerateServerPrinterUrlWithValidScheme(
    const std::string& url) {
  std::optional<GURL> gurl = std::make_optional(GURL(url));
  if (!HasValidServerPrinterScheme(*gurl)) {
    // If we're missing a valid scheme, try querying with IPPS first.
    gurl = GURL("ipps://" + url);
  }

  if (!gurl->is_valid()) {
    return std::nullopt;
  }

  // Replaces IPP/IPPS by HTTP/HTTPS. IPP standard describes protocol built
  // on top of HTTP, so both types of addresses have the same meaning in the
  // context of IPP interface. Moreover, the URL must have HTTP/HTTPS scheme
  // to pass IsStandard() test from GURL library (see "Validation of the URL
  // address" below).
  if (gurl->SchemeIs("ipp")) {
    gurl = UpdateServerPrinterGURL(*gurl, "http",
                                   /*replace_ipp_port=*/false);
    // The default port for IPP is 631. If the schema IPP is replaced by HTTP
    // and the port is not explicitly defined in the URL, we have to overwrite
    // the default HTTP port with the default IPP port. For IPPS we do nothing
    // because implementers use the same port for IPPS and HTTPS.
    if (gurl->IntPort() == url::PORT_UNSPECIFIED) {
      gurl = UpdateServerPrinterGURL(*gurl, /*scheme=*/"",
                                     /*replace_ipp_port=*/true);
    }
  } else if (gurl->SchemeIs("ipps")) {
    gurl = UpdateServerPrinterGURL(*gurl, "https",
                                   /*replace_ipp_port=*/false);
  }

  // Check validation of the URL address and return |gurl| if valid.
  return gurl->IsStandard() ? gurl : std::nullopt;
}

}  // namespace ash::settings
