// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ELEVATION_ICON_SETTER_H_
#define CHROME_BROWSER_UI_VIEWS_ELEVATION_ICON_SETTER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

class SkBitmap;

namespace views {
class LabelButton;
}

// On Windows, badges a button with a "UAC shield" icon to indicate that
// clicking will trigger a UAC elevation prompt.  Does nothing on other
// platforms.
class ElevationIconSetter {
 public:
  // |button| must be guaranteed to be alive throughout this class' lifetime!
  // |callback| will be called if the button icon is actually changed; callers
  // should pass a function which does a relayout on the view containing the
  // button, to ensure the button is correctly resized as necessary.
  explicit ElevationIconSetter(views::LabelButton* button);

  ElevationIconSetter(const ElevationIconSetter&) = delete;
  ElevationIconSetter& operator=(const ElevationIconSetter&) = delete;

  ~ElevationIconSetter();

 private:
  void SetButtonIcon(const SkBitmap& icon);

  raw_ptr<views::LabelButton> button_;
  base::WeakPtrFactory<ElevationIconSetter> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_ELEVATION_ICON_SETTER_H_
