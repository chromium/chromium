// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTKUI_GTK_UTIL_H_
#define CHROME_BROWSER_UI_LIBGTKUI_GTK_UTIL_H_

#include <gtk/gtk.h>
#include <string>

#include "ui/base/glib/scoped_gobject.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/window/frame_buttons.h"

typedef union _GdkEvent GdkEvent;

namespace aura {
class Window;
}

namespace base {
class CommandLine;
}

namespace color_utils {
struct HSL;
}

namespace ui {
class KeyEvent;
}

namespace libgtkui {

extern const color_utils::HSL kDefaultTintFrameIncognito;

void GtkInitFromCommandLine(const base::CommandLine& command_line);

// Sets |dialog| as transient for |parent|, which will keep it on top and center
// it above |parent|. Do nothing if |parent| is nullptr.
void SetGtkTransientForAura(GtkWidget* dialog, aura::Window* parent);

// Gets the transient parent aura window for |dialog|.
aura::Window* GetAuraTransientParent(GtkWidget* dialog);

// Clears the transient parent for |dialog|.
void ClearAuraTransientParent(GtkWidget* dialog);

// Parses |button_string| into |leading_buttons| and
// |trailing_buttons|.  The string is of the format
// "<button>*:<button*>", for example, "close:minimize:maximize".
// This format is used by GTK settings and gsettings.
void ParseButtonLayout(const std::string& button_string,
                       std::vector<views::FrameButton>* leading_buttons,
                       std::vector<views::FrameButton>* trailing_buttons);

void* GetGtkSharedLibrary();

class CairoSurface {
 public:
  // Attaches a cairo surface to an SkBitmap so that GTK can render
  // into it.  |bitmap| must outlive this CairoSurface.
  explicit CairoSurface(SkBitmap& bitmap);

  // Creates a new cairo surface with the given size.  The memory for
  // this surface is deallocated when this CairoSurface is destroyed.
  explicit CairoSurface(const gfx::Size& size);

  ~CairoSurface();

  // Get the drawing context for GTK to use.
  cairo_t* cairo() { return cairo_; }

  // Returns the average of all pixels in the surface.  If |frame| is
  // true, the resulting alpha will be the average alpha, otherwise it
  // will be the max alpha across all pixels.
  SkColor GetAveragePixelValue(bool frame);

 private:
  cairo_surface_t* surface_;
  cairo_t* cairo_;
};

// Returns true iff the runtime version of Gtk used meets
// |major|.|minor|.|micro|.
bool GtkVersionCheck(int major, int minor = 0, int micro = 0);

using ScopedStyleContext = ScopedGObject<GtkStyleContext>;
using ScopedCssProvider = ScopedGObject<GtkCssProvider>;

}  // namespace libgtkui

// Template override cannot be in the libgtkui namespace.
template <>
inline void libgtkui::ScopedStyleContext::Unref() {
  // Versions of GTK earlier than 3.15.4 had a bug where a g_assert
  // would be triggered when trying to free a GtkStyleContext that had
  // a parent whose only reference was the child context in question.
  // This is a hack to work around that case.  See GTK commit
  // "gtkstylecontext: Don't try to emit a signal when finalizing".
  GtkStyleContext* context = obj_;
  while (context) {
    GtkStyleContext* parent = gtk_style_context_get_parent(context);
    if (parent && G_OBJECT(context)->ref_count == 1 &&
        !libgtkui::GtkVersionCheck(3, 15, 4)) {
      g_object_ref(parent);
      gtk_style_context_set_parent(context, nullptr);
      g_object_unref(context);
    } else {
      g_object_unref(context);
      return;
    }
    context = parent;
  }
}

namespace libgtkui {

// Converts ui::NativeTheme::State to GtkStateFlags.
GtkStateFlags StateToStateFlags(ui::NativeTheme::State state);

// If |context| is nullptr, creates a new top-level style context
// specified by parsing |css_node|.  Otherwise, creates the child
// context with |context| as the parent.
ScopedStyleContext AppendCssNodeToStyleContext(GtkStyleContext* context,
                                               const std::string& css_node);

// Parses |css_selector| into a GtkStyleContext.  The format is a
// sequence of whitespace-separated objects.  Each object may have at
// most one object name at the beginning of the string, and any number
// of '.'-prefixed classes and ':'-prefixed pseudoclasses.  An example
// is "GtkButton.button.suggested-action:hover:active".  The caller
// must g_object_unref() the returned context.
ScopedStyleContext GetStyleContextFromCss(const std::string& css_selector);

SkColor GetFgColorFromStyleContext(GtkStyleContext* context);

SkColor GetBgColorFromStyleContext(GtkStyleContext* context);

// Overrides properties on |context| and all its parents with those
// provided by |css|.
void ApplyCssToContext(GtkStyleContext* context, const std::string& css);

// Get the 'color' property from the style context created by
// GetStyleContextFromCss(|css_selector|).
SkColor GetFgColor(const std::string& css_selector);

ScopedCssProvider GetCssProvider(const std::string& css);

// Renders the backgrounds of all ancestors of |context|, then renders
// the background for |context| itself.
void RenderBackground(const gfx::Size& size,
                      cairo_t* cr,
                      GtkStyleContext* context);

// Renders a background from the style context created by
// GetStyleContextFromCss(|css_selector|) into a 24x24 bitmap and
// returns the average color.
SkColor GetBgColor(const std::string& css_selector);

// Renders the border from the style context created by
// GetStyleContextFromCss(|css_selector|) into a 24x24 bitmap and
// returns the average color.
SkColor GetBorderColor(const std::string& css_selector);

// On Gtk3.20 or later, behaves like GetBgColor.  Otherwise, returns
// the background-color property.
SkColor GetSelectionBgColor(const std::string& css_selector);

// Get the color of the GtkSeparator specified by |css_selector|.
SkColor GetSeparatorColor(const std::string& css_selector);

// Get a GtkSettings property as a C++ string.
std::string GetGtkSettingsStringProperty(GtkSettings* settings,
                                         const gchar* prop_name);

// Get current GdkDisplay instance
GdkDisplay* GetGdkDisplay();

// Xkb Events store group attribute into XKeyEvent::state bit field, along with
// other state-related info, while GdkEventKey objects have separate fields for
// that purpose, they are ::state and ::group. This function is responsible for
// recomposing them into a single bit field value when translating GdkEventKey
// into XKeyEvent. This is similar to XkbBuildCoreState(), but assumes state is
// an uint rather than an uchar.
//
// More details:
// https://gitlab.freedesktop.org/xorg/proto/xorgproto/blob/master/include/X11/extensions/XKB.h#L372
int BuildXkbStateFromGdkEvent(unsigned int state, unsigned char group);

// Translates |key_event| into a GdkEvent. GdkEvent::key::window is the only
// field not set by this function, callers must set it, as the way for
// retrieving it may vary depending on the event being processed. E.g: for IME
// Context impl, X11 window XID is obtained through Event::target() which is
// root aura::Window targeted by that key event.
GdkEvent* GdkEventFromKeyEvent(const ui::KeyEvent& key_event);

}  // namespace libgtkui

#endif  // CHROME_BROWSER_UI_LIBGTKUI_GTK_UTIL_H_
