// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_QUERY_PARAMS_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_QUERY_PARAMS_H_

#include <string>

#include "base/macros.h"

namespace update_client {

class UpdateQueryParamsDelegate;

// Generates a string of URL query parameters to be used when getting
// component and extension updates. These parameters generally remain
// fixed for a particular build. Embedders can use the delegate to
// define different implementations. This should be used only in the
// browser process.
class UpdateQueryParams {
 public:
  enum ProdId {
    CHROME = 0,
    CRX,
  };

  // Generates a string of URL query parameters for Omaha. Includes the
  // following fields: "os", "arch", "nacl_arch", "prod", "prodchannel",
  // "prodversion", and "lang"
  static std::string Get(ProdId prod);

  // Returns the value we use for the "prod=" parameter. Possible return values
  // include "chrome", "chromecrx", "chromiumcrx", and "unknown".
  static const char* GetProdIdString(ProdId prod);

  // Returns the value we use for the "os=" parameter. Possible return values
  // include: "mac", "win", "android", "cros", "linux", and "openbsd".
  static const char* GetOS();

  // Returns the value we use for the "arch=" parameter. Possible return values
  // include: "x86", "x64", and "arm".
  static const char* GetArch();

  // Returns the value we use for the "nacl_arch" parameter. Note that this may
  // be different from the "arch" parameter above (e.g. one may be 32-bit and
  // the other 64-bit). Possible return values include: "x86-32", "x86-64",
  // "arm", "mips32", and "ppc64".
  static const char* GetNaclArch();

  // Returns the current version of Chrome/Chromium.
  static std::string GetProdVersion();

  // Use this delegate.
  static void SetDelegate(UpdateQueryParamsDelegate* delegate);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(UpdateQueryParams);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_QUERY_PARAMS_H_
