// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_LANGUAGES_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_LANGUAGES_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/lens/core/mojom/translate.mojom.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/simple_url_loader.h"

class Profile;

// Callback type alias for page content bytes retrieved.
using SupportedLanguagesRetrievedCallback =
    base::OnceCallback<void(const std::string&,
                            std::vector<lens::mojom::LanguagePtr>,
                            std::vector<lens::mojom::LanguagePtr>)>;

namespace lens {

// Sends a request to get supported translate source and target languages for
// the Lens Overlay.
class LensOverlayLanguagesController {
 public:
  explicit LensOverlayLanguagesController(Profile* profile);
  virtual ~LensOverlayLanguagesController();

  void SendGetSupportedLanguagesRequest(
      SupportedLanguagesRetrievedCallback callback);

 private:
  std::unique_ptr<network::SimpleURLLoader> InitializeURLLoader();

  void OnGetSupportedLanguagesResponse(
      std::optional<std::string> response_body);

  void OnJsonParsed(data_decoder::DataDecoder::ValueOrError result);

  std::vector<lens::mojom::LanguagePtr> RetrieveLanguagesFromResults(
      const base::Value::List* result_list);

  // Callback for when the get supported languages response is decoded.
  SupportedLanguagesRetrievedCallback callback_;

  // The profile used to make requests.
  raw_ptr<Profile> profile_;

  // The locale used in the language request. Stored since technically, the
  // locale can change before a response was received.
  std::string locale_;

  // A url loader to load the request to get supported languages.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::WeakPtrFactory<LensOverlayLanguagesController> weak_ptr_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_LANGUAGES_CONTROLLER_H_
