// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtkui/gtk_util.h"

#include <dlfcn.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <stddef.h>

#include <memory>

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/environment.h"
#include "base/memory/protected_memory.h"
#include "base/memory/protected_memory_cfi.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/linux_ui/linux_ui.h"

namespace {

void CommonInitFromCommandLine(const base::CommandLine& command_line) {
#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_init();
#else
  const std::vector<std::string>& args = command_line.argv();
  int argc = args.size();
  std::unique_ptr<char* []> argv(new char*[argc + 1]);
  for (size_t i = 0; i < args.size(); ++i) {
    // TODO(piman@google.com): can gtk_init modify argv? Just being safe
    // here.
    argv[i] = strdup(args[i].c_str());
  }
  argv[argc] = nullptr;
  char** argv_pointer = argv.get();

  {
    // http://crbug.com/423873
    ANNOTATE_SCOPED_MEMORY_LEAK;
    gtk_init(&argc, &argv_pointer);
  }
  for (size_t i = 0; i < args.size(); ++i) {
    free(argv[i]);
  }
#endif
}

}  // namespace

namespace libgtkui {

// TODO(thomasanderson): ThemeService has a whole interface just for reading
// default constants. Figure out what to do with that more long term; for now,
// just copy the constant itself here.
const color_utils::HSL kDefaultTintFrameIncognito = {-1, 0.2f, 0.35f};

void GtkInitFromCommandLine(const base::CommandLine& command_line) {
  CommonInitFromCommandLine(command_line);
}

// TODO(erg): This method was copied out of shell_integration_linux.cc. Because
// of how this library is structured as a stand alone .so, we can't call code
// from browser and above.
std::string GetDesktopName(base::Environment* env) {
#if defined(GOOGLE_CHROME_BUILD)
  return "google-chrome.desktop";
#else  // CHROMIUM_BUILD
  // Allow $CHROME_DESKTOP to override the built-in value, so that development
  // versions can set themselves as the default without interfering with
  // non-official, packaged versions using the built-in value.
  std::string name;
  if (env->GetVar("CHROME_DESKTOP", &name) && !name.empty())
    return name;
  return "chromium-browser.desktop";
#endif
}

guint GetGdkKeyCodeForAccelerator(const ui::Accelerator& accelerator) {
  // The second parameter is false because accelerator keys are expressed in
  // terms of the non-shift-modified key.
  return XKeysymForWindowsKeyCode(accelerator.key_code(), false);
}

GdkModifierType GetGdkModifierForAccelerator(
    const ui::Accelerator& accelerator) {
  int event_flag = accelerator.modifiers();
  int modifier = 0;
  if (event_flag & ui::EF_SHIFT_DOWN)
    modifier |= GDK_SHIFT_MASK;
  if (event_flag & ui::EF_CONTROL_DOWN)
    modifier |= GDK_CONTROL_MASK;
  if (event_flag & ui::EF_ALT_DOWN)
    modifier |= GDK_MOD1_MASK;
  return static_cast<GdkModifierType>(modifier);
}

int EventFlagsFromGdkState(guint state) {
  int flags = ui::EF_NONE;
  flags |= (state & GDK_SHIFT_MASK) ? ui::EF_SHIFT_DOWN : ui::EF_NONE;
  flags |= (state & GDK_LOCK_MASK) ? ui::EF_CAPS_LOCK_ON : ui::EF_NONE;
  flags |= (state & GDK_CONTROL_MASK) ? ui::EF_CONTROL_DOWN : ui::EF_NONE;
  flags |= (state & GDK_MOD1_MASK) ? ui::EF_ALT_DOWN : ui::EF_NONE;
  flags |= (state & GDK_BUTTON1_MASK) ? ui::EF_LEFT_MOUSE_BUTTON : ui::EF_NONE;
  flags |=
      (state & GDK_BUTTON2_MASK) ? ui::EF_MIDDLE_MOUSE_BUTTON : ui::EF_NONE;
  flags |= (state & GDK_BUTTON3_MASK) ? ui::EF_RIGHT_MOUSE_BUTTON : ui::EF_NONE;
  return flags;
}

void TurnButtonBlue(GtkWidget* button) {
  gtk_style_context_add_class(gtk_widget_get_style_context(button),
                              "suggested-action");
}

void SetGtkTransientForAura(GtkWidget* dialog, aura::Window* parent) {
  if (!parent || !parent->GetHost())
    return;

  gtk_widget_realize(dialog);
  GdkWindow* gdk_window = gtk_widget_get_window(dialog);

  // TODO(erg): Check to make sure we're using X11 if wayland or some other
  // display server ever happens. Otherwise, this will crash.
  XSetTransientForHint(GDK_WINDOW_XDISPLAY(gdk_window),
                       GDK_WINDOW_XID(gdk_window),
                       parent->GetHost()->GetAcceleratedWidget());
}

void ParseButtonLayout(const std::string& button_string,
                       std::vector<views::FrameButton>* leading_buttons,
                       std::vector<views::FrameButton>* trailing_buttons) {
  leading_buttons->clear();
  trailing_buttons->clear();
  bool left_side = true;
  base::StringTokenizer tokenizer(button_string, ":,");
  tokenizer.set_options(base::StringTokenizer::RETURN_DELIMS);
  while (tokenizer.GetNext()) {
    if (tokenizer.token_is_delim()) {
      if (*tokenizer.token_begin() == ':')
        left_side = false;
    } else {
      base::StringPiece token = tokenizer.token_piece();
      if (token == "minimize") {
        (left_side ? leading_buttons : trailing_buttons)
            ->push_back(views::FRAME_BUTTON_MINIMIZE);
      } else if (token == "maximize") {
        (left_side ? leading_buttons : trailing_buttons)
            ->push_back(views::FRAME_BUTTON_MAXIMIZE);
      } else if (token == "close") {
        (left_side ? leading_buttons : trailing_buttons)
            ->push_back(views::FRAME_BUTTON_CLOSE);
      }
    }
  }
}

namespace {

float GetDeviceScaleFactor() {
  views::LinuxUI* linux_ui = views::LinuxUI::instance();
  return linux_ui ? linux_ui->GetDeviceScaleFactor() : 1;
}

using GtkSetState = void (*)(GtkWidgetPath*, gint, GtkStateFlags);
PROTECTED_MEMORY_SECTION base::ProtectedMemory<GtkSetState>
    _gtk_widget_path_iter_set_state;

using GtkSetObjectName = void (*)(GtkWidgetPath*, gint, const char*);
PROTECTED_MEMORY_SECTION base::ProtectedMemory<GtkSetObjectName>
    _gtk_widget_path_iter_set_object_name;

}  // namespace

void* GetGdkSharedLibrary() {
  std::string lib_name =
      "libgdk-" + std::to_string(GTK_MAJOR_VERSION) + ".so.0";
  static void* gdk_lib = dlopen(lib_name.c_str(), RTLD_LAZY);
  DCHECK(gdk_lib);
  return gdk_lib;
}

void* GetGtkSharedLibrary() {
  std::string lib_name =
      "libgtk-" + std::to_string(GTK_MAJOR_VERSION) + ".so.0";
  static void* gtk_lib = dlopen(lib_name.c_str(), RTLD_LAZY);
  DCHECK(gtk_lib);
  return gtk_lib;
}

CairoSurface::CairoSurface(SkBitmap& bitmap)
    : surface_(cairo_image_surface_create_for_data(
          static_cast<unsigned char*>(bitmap.getAddr(0, 0)),
          CAIRO_FORMAT_ARGB32,
          bitmap.width(),
          bitmap.height(),
          cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, bitmap.width()))),
      cairo_(cairo_create(surface_)) {}

CairoSurface::CairoSurface(const gfx::Size& size)
    : surface_(cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                          size.width(),
                                          size.height())),
      cairo_(cairo_create(surface_)) {
  DCHECK(cairo_surface_status(surface_) == CAIRO_STATUS_SUCCESS);
  // Clear the surface.
  cairo_save(cairo_);
  cairo_set_source_rgba(cairo_, 0, 0, 0, 0);
  cairo_set_operator(cairo_, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cairo_);
  cairo_restore(cairo_);
}

CairoSurface::~CairoSurface() {
  cairo_destroy(cairo_);
  cairo_surface_destroy(surface_);
}

SkColor CairoSurface::GetAveragePixelValue(bool frame) {
  cairo_surface_flush(surface_);
  SkColor* data =
      reinterpret_cast<SkColor*>(cairo_image_surface_get_data(surface_));
  int width = cairo_image_surface_get_width(surface_);
  int height = cairo_image_surface_get_height(surface_);
  DCHECK(4 * width == cairo_image_surface_get_stride(surface_));
  long a = 0, r = 0, g = 0, b = 0;
  unsigned int max_alpha = 0;
  for (int i = 0; i < width * height; i++) {
    SkColor color = data[i];
    max_alpha = std::max(SkColorGetA(color), max_alpha);
    a += SkColorGetA(color);
    r += SkColorGetR(color);
    g += SkColorGetG(color);
    b += SkColorGetB(color);
  }
  if (a == 0)
    return SK_ColorTRANSPARENT;
  return SkColorSetARGB(frame ? max_alpha : a / (width * height), r * 255 / a,
                        g * 255 / a, b * 255 / a);
}

bool GtkVersionCheck(int major, int minor, int micro) {
  static int actual_major = gtk_get_major_version();
  if (actual_major > major)
    return true;
  else if (actual_major < major)
    return false;

  static int actual_minor = gtk_get_minor_version();
  if (actual_minor > minor)
    return true;
  else if (actual_minor < minor)
    return false;

  static int actual_micro = gtk_get_micro_version();
  if (actual_micro >= micro)
    return true;
  else
    return false;
}

GtkStateFlags StateToStateFlags(ui::NativeTheme::State state) {
  switch (state) {
    case ui::NativeTheme::kDisabled:
      return GTK_STATE_FLAG_INSENSITIVE;
    case ui::NativeTheme::kHovered:
      return GTK_STATE_FLAG_PRELIGHT;
    case ui::NativeTheme::kNormal:
      return GTK_STATE_FLAG_NORMAL;
    case ui::NativeTheme::kPressed:
      return static_cast<GtkStateFlags>(GTK_STATE_FLAG_PRELIGHT |
                                        GTK_STATE_FLAG_ACTIVE);
    default:
      NOTREACHED();
      return GTK_STATE_FLAG_NORMAL;
  }
}

ScopedStyleContext AppendCssNodeToStyleContext(GtkStyleContext* context,
                                               const std::string& css_node) {
  GtkWidgetPath* path =
      context ? gtk_widget_path_copy(gtk_style_context_get_path(context))
              : gtk_widget_path_new();

  enum {
    CSS_TYPE,
    CSS_NAME,
    CSS_CLASS,
    CSS_PSEUDOCLASS,
  } part_type = CSS_TYPE;
  static const struct {
    const char* name;
    GtkStateFlags state_flag;
  } pseudo_classes[] = {
      {"active", GTK_STATE_FLAG_ACTIVE},
      {"hover", GTK_STATE_FLAG_PRELIGHT},
      {"selected", GTK_STATE_FLAG_SELECTED},
      {"disabled", GTK_STATE_FLAG_INSENSITIVE},
      {"indeterminate", GTK_STATE_FLAG_INCONSISTENT},
      {"focus", GTK_STATE_FLAG_FOCUSED},
      {"backdrop", GTK_STATE_FLAG_BACKDROP},
      {"link", GTK_STATE_FLAG_LINK},
      {"visited", GTK_STATE_FLAG_VISITED},
      {"checked", GTK_STATE_FLAG_CHECKED},
  };
  GtkStateFlags state = GTK_STATE_FLAG_NORMAL;
  base::StringTokenizer t(css_node, ".:#");
  t.set_options(base::StringTokenizer::RETURN_DELIMS);
  while (t.GetNext()) {
    if (t.token_is_delim()) {
      if (t.token_begin() == css_node.begin()) {
        // Special case for the first token.
        gtk_widget_path_append_type(path, G_TYPE_NONE);
      }
      switch (*t.token_begin()) {
        case '#':
          part_type = CSS_NAME;
          break;
        case '.':
          part_type = CSS_CLASS;
          break;
        case ':':
          part_type = CSS_PSEUDOCLASS;
          break;
        default:
          NOTREACHED();
      }
    } else {
      static base::ProtectedMemory<GtkSetObjectName>::Initializer init(
          &_gtk_widget_path_iter_set_object_name,
          reinterpret_cast<GtkSetObjectName>(dlsym(
              GetGtkSharedLibrary(), "gtk_widget_path_iter_set_object_name")));
      switch (part_type) {
        case CSS_NAME: {
          if (GtkVersionCheck(3, 20)) {
            base::UnsanitizedCfiCall(_gtk_widget_path_iter_set_object_name)(
                path, -1, t.token().c_str());
          } else {
            gtk_widget_path_iter_add_class(path, -1, t.token().c_str());
          }
          break;
        }
        case CSS_TYPE: {
          GType type = g_type_from_name(t.token().c_str());
          DCHECK(type);
          gtk_widget_path_append_type(path, type);
          if (GtkVersionCheck(3, 20)) {
            if (t.token() == "GtkLabel")
              base::UnsanitizedCfiCall(_gtk_widget_path_iter_set_object_name)(
                  path, -1, "label");
          }
          break;
        }
        case CSS_CLASS: {
          gtk_widget_path_iter_add_class(path, -1, t.token().c_str());
          break;
        }
        case CSS_PSEUDOCLASS: {
          GtkStateFlags state_flag = GTK_STATE_FLAG_NORMAL;
          for (const auto& pseudo_class_entry : pseudo_classes) {
            if (strcmp(pseudo_class_entry.name, t.token().c_str()) == 0) {
              state_flag = pseudo_class_entry.state_flag;
              break;
            }
          }
          state = static_cast<GtkStateFlags>(state | state_flag);
          break;
        }
      }
    }
  }

  // Always add a "chromium" class so that themes can style chromium
  // widgets specially if they want to.
  gtk_widget_path_iter_add_class(path, -1, "chromium");

  if (GtkVersionCheck(3, 14)) {
    static base::ProtectedMemory<GtkSetState>::Initializer init(
        &_gtk_widget_path_iter_set_state,
        reinterpret_cast<GtkSetState>(
            dlsym(GetGtkSharedLibrary(), "gtk_widget_path_iter_set_state")));
    DCHECK(*_gtk_widget_path_iter_set_state);
    base::UnsanitizedCfiCall(_gtk_widget_path_iter_set_state)(path, -1, state);
  }

  ScopedStyleContext child_context(gtk_style_context_new());
  gtk_style_context_set_path(child_context, path);
  if (GtkVersionCheck(3, 14)) {
    gtk_style_context_set_state(child_context, state);
  } else {
    GtkStateFlags child_state = state;
    if (context) {
      child_state = static_cast<GtkStateFlags>(
          child_state | gtk_style_context_get_state(context));
    }
    gtk_style_context_set_state(child_context, child_state);
  }
  gtk_style_context_set_scale(child_context, std::ceil(GetDeviceScaleFactor()));
  gtk_style_context_set_parent(child_context, context);
  gtk_widget_path_unref(path);
  return child_context;
}

ScopedStyleContext GetStyleContextFromCss(const std::string& css_selector) {
  // Prepend a window node to the selector since all widgets must live
  // in a window, but we don't want to specify that every time.
  auto context =
      AppendCssNodeToStyleContext(nullptr, "GtkWindow#window.background");

  for (const auto& widget_type :
       base::SplitString(css_selector, base::kWhitespaceASCII,
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    context = AppendCssNodeToStyleContext(context, widget_type);
  }
  return context;
}

SkColor GdkRgbaToSkColor(const GdkRGBA& color) {
  return SkColorSetARGB(color.alpha * 255, color.red * 255, color.green * 255,
                        color.blue * 255);
}

SkColor GetFgColorFromStyleContext(GtkStyleContext* context) {
  GdkRGBA color;
#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_style_context_get_color(context, &color);
#else
  gtk_style_context_get_color(context, gtk_style_context_get_state(context),
                              &color);
#endif
  return GdkRgbaToSkColor(color);
}

SkColor GetBgColorFromStyleContext(GtkStyleContext* context) {
  // Backgrounds are more general than solid colors (eg. gradients),
  // but chromium requires us to boil this down to one color.  We
  // cannot use the background-color here because some themes leave it
  // set to a garbage color because a background-image will cover it
  // anyway.  So we instead render the background into a 24x24 bitmap,
  // removing any borders, and hope that we get a good color.
  ApplyCssToContext(context,
                    "* {"
                    "border-radius: 0px;"
                    "border-style: none;"
                    "box-shadow: none;"
                    "}");
  gfx::Size size(24, 24);
  CairoSurface surface(size);
  RenderBackground(size, surface.cairo(), context);
  return surface.GetAveragePixelValue(false);
}

SkColor GetFgColor(const std::string& css_selector) {
  return GetFgColorFromStyleContext(GetStyleContextFromCss(css_selector));
}

ScopedCssProvider GetCssProvider(const std::string& css) {
  GtkCssProvider* provider = gtk_css_provider_new();
#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_css_provider_load_from_data(provider, css.c_str(), -1);
#else
  GError* error = nullptr;
  gtk_css_provider_load_from_data(provider, css.c_str(), -1, &error);
  DCHECK(!error);
#endif
  return ScopedCssProvider(provider);
}

void ApplyCssProviderToContext(GtkStyleContext* context,
                               GtkCssProvider* provider) {
  while (context) {
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider),
                                   G_MAXUINT);
    context = gtk_style_context_get_parent(context);
  }
}

void ApplyCssToContext(GtkStyleContext* context, const std::string& css) {
  auto provider = GetCssProvider(css);
  ApplyCssProviderToContext(context, provider);
}

void RenderBackground(const gfx::Size& size,
                      cairo_t* cr,
                      GtkStyleContext* context) {
  if (!context)
    return;
  RenderBackground(size, cr, gtk_style_context_get_parent(context));
  gtk_render_background(context, cr, 0, 0, size.width(), size.height());
}

SkColor GetBgColor(const std::string& css_selector) {
  return GetBgColorFromStyleContext(GetStyleContextFromCss(css_selector));
}

SkColor GetBorderColor(const std::string& css_selector) {
  // Borders have the same issue as backgrounds, due to the
  // border-image property.
  auto context = GetStyleContextFromCss(css_selector);
  gfx::Size size(24, 24);
  CairoSurface surface(size);
  gtk_render_frame(context, surface.cairo(), 0, 0, size.width(), size.height());
  return surface.GetAveragePixelValue(true);
}

SkColor GetSelectionBgColor(const std::string& css_selector) {
  auto context = GetStyleContextFromCss(css_selector);
  if (GtkVersionCheck(3, 20))
    return GetBgColorFromStyleContext(context);
  // This is verbatim how Gtk gets the selection color on versions before 3.20.
  GdkRGBA selection_color;
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_style_context_get_background_color(context, &selection_color);
#else
  gtk_style_context_get_background_color(
      context, gtk_style_context_get_state(context), &selection_color);
#endif
  G_GNUC_END_IGNORE_DEPRECATIONS;
  return GdkRgbaToSkColor(selection_color);
}

bool ContextHasClass(GtkStyleContext* context, const std::string& style_class) {
  return gtk_style_context_has_class(context, style_class.c_str()) ||
         gtk_widget_path_iter_has_class(gtk_style_context_get_path(context), -1,
                                        style_class.c_str());
}

SkColor GetSeparatorColor(const std::string& css_selector) {
  if (!GtkVersionCheck(3, 20))
    return GetFgColor(css_selector);

  auto context = GetStyleContextFromCss(css_selector);
  int w = 1, h = 1;
  GtkBorder border, padding;
#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_style_context_get(context, "min-width", &w, "min-height", &h, nullptr);
  gtk_style_context_get_border(context, &border);
  gtk_style_context_get_padding(context, &padding);
#else
  gtk_style_context_get(context, gtk_style_context_get_state(context),
                        "min-width", &w, "min-height", &h, nullptr);
  GtkStateFlags state = gtk_style_context_get_state(context);
  gtk_style_context_get_border(context, state, &border);
  gtk_style_context_get_padding(context, state, &padding);
#endif
  w += border.left + padding.left + padding.right + border.right;
  h += border.top + padding.top + padding.bottom + border.bottom;

  bool horizontal = ContextHasClass(context, "horizontal");
  if (horizontal) {
    w = 24;
    h = std::max(h, 1);
  } else {
    DCHECK(ContextHasClass(context, "vertical"));
    h = 24;
    w = std::max(w, 1);
  }

  CairoSurface surface(gfx::Size(w, h));
  gtk_render_background(context, surface.cairo(), 0, 0, w, h);
  gtk_render_frame(context, surface.cairo(), 0, 0, w, h);
  return surface.GetAveragePixelValue(false);
}

}  // namespace libgtkui
