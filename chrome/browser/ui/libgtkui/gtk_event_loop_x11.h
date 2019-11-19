// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTKUI_GTK_EVENT_LOOP_X11_H_
#define CHROME_BROWSER_UI_LIBGTKUI_GTK_EVENT_LOOP_X11_H_

#include "base/macros.h"
#include "ui/base/glib/glib_integers.h"

typedef union _GdkEvent GdkEvent;
typedef struct _GdkEventKey GdkEventKey;

namespace base {
template <typename Type>
struct DefaultSingletonTraits;
}

namespace libgtkui {

class GtkEventLoopX11 {
 public:
  static GtkEventLoopX11* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<GtkEventLoopX11>;

  GtkEventLoopX11();
  ~GtkEventLoopX11();

  static void DispatchGdkEvent(GdkEvent* gdk_event, gpointer);
  static void ProcessGdkEventKey(const GdkEventKey& gdk_event_key);

  DISALLOW_COPY_AND_ASSIGN(GtkEventLoopX11);
};

}  // namespace libgtkui

#endif  // CHROME_BROWSER_UI_LIBGTKUI_GTK_EVENT_LOOP_X11_H_
