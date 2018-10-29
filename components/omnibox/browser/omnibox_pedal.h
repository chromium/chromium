// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_H_

#include <unordered_set>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "url/gurl.h"

class OmniboxEditController;
class OmniboxClient;

// Conceptually, a Pedal is a fixed action that can be taken by the user
// pressing a button or taking a new dedicated suggestion when some
// associated intention is detected in an omnibox match suggestion.
// The typical action is to navigate to an internal Chrome surface
// like settings, but other actions like translation or launching
// an incognito window are possible.  The intention is detected by
// checking trigger queries against suggested match queries.
class OmniboxPedal {
 public:
  struct LabelStrings {
    LabelStrings(int id_hint, int id_hint_short, int id_suggestion_contents);
    const base::string16 hint;
    const base::string16 hint_short;
    const base::string16 suggestion_contents;
  };

  // ExecutionContext provides the necessary structure for Pedal
  // execution implementations that potentially vary widely, and
  // references are preferred over pointers for members that are
  // not nullable.  If there's ever a good case for changing to
  // nullable pointers, this can change but for now presence is
  // guaranteed so Pedals can use them without needing to check.
  // The other reason to use a context is that it's easier to
  // maintain with lots of Pedal implementations because the amount
  // of boilerplate required is greatly reduced.
  class ExecutionContext {
   public:
    ExecutionContext(OmniboxClient& client,
                     OmniboxEditController& controller,
                     base::TimeTicks match_selection_timestamp)
        : client_(client),
          controller_(controller),
          match_selection_timestamp_(match_selection_timestamp) {}
    OmniboxClient& client_;
    OmniboxEditController& controller_;
    base::TimeTicks match_selection_timestamp_;
  };

  OmniboxPedal(LabelStrings strings);
  virtual ~OmniboxPedal();

  // Provides read access to labels associated with this Pedal.
  const LabelStrings& GetLabelStrings() const;

  // Returns true if this is purely a navigation Pedal with URL.
  bool IsNavigation() const;

  // For navigation Pedals, returns the destination URL.
  const GURL& GetNavigationUrl() const;

  // These Should* methods can likely be eliminated when Pedal
  // suggestion mode is firmly established.

  // When a suggestion is selected by user, it may be via button press or
  // normal click/keypress.  This method tells whether the mode of selection
  // taken should result in execution of the suggestion's Pedal.
  virtual bool ShouldExecute(bool button_pressed) const;

  // Some Pedals (or all, depending on mode) may be presented with a side
  // button; this method returns true if this Pedal presents a button.
  virtual bool ShouldPresentButton() const;

  // Takes the action associated with this Pedal.  Non-navigation
  // Pedals must override the default, but Navigation Pedals don't need to.
  virtual void Execute(ExecutionContext& context) const;

  // Returns true if the preprocessed match suggestion text triggers
  // presentation of this Pedal.  This is not intended for general use,
  // and only OmniboxPedalProvider should need to call this method.
  bool IsTriggerMatch(const base::string16& match_text) const;

 protected:
  // Use this for the common case of navigating to a URL.
  void OpenURL(ExecutionContext& context, const GURL& url) const;

  std::unordered_set<base::string16> triggers_;
  LabelStrings strings_;

  // For navigation Pedals, this holds the destination URL; for action Pedals,
  // this remains empty.
  GURL url_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_H_
