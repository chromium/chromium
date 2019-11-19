// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_REMOVE_SUGGESTION_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_REMOVE_SUGGESTION_BUBBLE_H_

#include "base/callback_forward.h"

class TemplateURLService;
struct AutocompleteMatch;

namespace views {
class View;
}

// Shows a confirmation bubble to remove a suggestion represented by |match|.
// If the user clicks Remove, then |remove_closure| is executed, and the bubble
// is closed.
void ShowRemoveSuggestion(TemplateURLService* template_url_service,
                          views::View* anchor_view,
                          const AutocompleteMatch& match,
                          base::OnceClosure remove_closure);

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_REMOVE_SUGGESTION_BUBBLE_H_
