// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_LENS_SEARCHBOX_CLIENT_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_LENS_SEARCHBOX_CLIENT_H_

#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/sessions/core/session_id.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "url/gurl.h"

// Interface that allows the Lens searchbox to interact with its embedder
// (i.e., LensOverlayController).
class LensSearchboxClient {
 public:
  LensSearchboxClient() = default;
  virtual ~LensSearchboxClient() = default;

  // Returns the URL of the current page in the WebContents.
  virtual const GURL& GetPageURL() const = 0;

  // Returns the current tab ID.
  virtual SessionID GetTabId() const = 0;

  // Returns the appropriate classification based on the current mode.
  virtual metrics::OmniboxEventProto::PageClassification GetPageClassification()
      const = 0;

  // Returns the thumbnail data (data:image/) or address (chrome://image/).
  virtual std::string& GetThumbnail() = 0;

  // Returns the Lens autocomplete inputs. Used to report iil=, vsrid=,
  // gsessionid=, and vsint= in the Suggest requests.
  virtual const lens::proto::LensOverlaySuggestInputs& GetLensSuggestInputs()
      const = 0;

  // Called when the user modifies the text in any way (add, delete, paste,
  // cut, etc.).
  virtual void OnTextModified() = 0;

  // Called when the user removes the thumbnail.
  virtual void OnThumbnailRemoved() = 0;

  // Called when a suggestion is accepted. Should open the given URL.
  virtual void OnSuggestionAccepted(const GURL& destination_url,
                                    AutocompleteMatchType::Type match_type,
                                    bool is_zero_prefix_suggestion) = 0;

  // Called when the handler binds to the remote page, aka when SetPage is set.
  virtual void OnPageBound() = 0;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_LENS_SEARCHBOX_CLIENT_H_
