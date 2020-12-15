// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_MODEL_SERVICE_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_MODEL_SERVICE_H_

#include "base/optional.h"

namespace base {
class File;
}  // namespace base

namespace translate {

// Service that manages models required to support translation in the browser.
class TranslateModelService {
 public:
  TranslateModelService() = default;

  // Returns a loaded file containing the TFLite model capable of detecting the
  // language of a web page's text.
  virtual base::Optional<base::File> GetLanguageDetectionModelFile() = 0;

 protected:
  virtual ~TranslateModelService() = default;
};

}  //  namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_MODEL_SERVICE_H_
