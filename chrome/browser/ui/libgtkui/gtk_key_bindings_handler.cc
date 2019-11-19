// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtkui/gtk_key_bindings_handler.h"

#include <gdk/gdkkeysyms.h>
#include <stddef.h>

#include <string>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/libgtkui/gtk_util.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/events/event.h"

using ui::TextEditCommand;

// TODO(erg): Rewrite the old gtk_key_bindings_handler_unittest.cc and get them
// in a state that links. This code was adapted from the content layer GTK
// code, which had some simple unit tests. However, the changes in the public
// interface basically meant the tests need to be rewritten; this imposes weird
// linking requirements regarding GTK+ as we don't have a libgtkui_unittests
// yet. http://crbug.com/358297.

namespace {

GtkWidget* CreateInvisibleWindow() {
#if GTK_CHECK_VERSION(3, 90, 0)
  return gtk_invisible_new();
#else
  return gtk_offscreen_window_new();
#endif
}

}  // namespace

namespace libgtkui {

GtkKeyBindingsHandler::GtkKeyBindingsHandler()
    : fake_window_(CreateInvisibleWindow()), handler_(CreateNewHandler()) {
  gtk_container_add(GTK_CONTAINER(fake_window_), handler_);
}

GtkKeyBindingsHandler::~GtkKeyBindingsHandler() {
  gtk_widget_destroy(handler_);
  gtk_widget_destroy(fake_window_);
}

bool GtkKeyBindingsHandler::MatchEvent(
    const ui::Event& event,
    std::vector<ui::TextEditCommandAuraLinux>* edit_commands) {
  CHECK(event.IsKeyEvent());

  const ui::KeyEvent& key_event = static_cast<const ui::KeyEvent&>(event);
  if (key_event.is_char())
    return false;

  GdkEvent* gdk_event = GdkEventFromKeyEvent(key_event);
  if (!gdk_event)
    return false;

  edit_commands_.clear();
  // If this key event matches a predefined key binding, corresponding signal
  // will be emitted.

  gtk_bindings_activate_event(G_OBJECT(handler_), &gdk_event->key);
  gdk_event_free(gdk_event);

  bool matched = !edit_commands_.empty();
  if (edit_commands)
    edit_commands->swap(edit_commands_);
  return matched;
}

GtkWidget* GtkKeyBindingsHandler::CreateNewHandler() {
  Handler* handler =
      static_cast<Handler*>(g_object_new(HandlerGetType(), nullptr));

  handler->owner = this;

  // We don't need to show the |handler| object on screen, so set its size to
  // zero.
  gtk_widget_set_size_request(GTK_WIDGET(handler), 0, 0);

  // Prevents it from handling any events by itself.
  gtk_widget_set_sensitive(GTK_WIDGET(handler), FALSE);
#if !GTK_CHECK_VERSION(3, 90, 0)
  gtk_widget_set_events(GTK_WIDGET(handler), 0);
#endif
  gtk_widget_set_can_focus(GTK_WIDGET(handler), TRUE);

  return GTK_WIDGET(handler);
}

void GtkKeyBindingsHandler::EditCommandMatched(TextEditCommand command,
                                               const std::string& value) {
  edit_commands_.push_back(ui::TextEditCommandAuraLinux(command, value));
}

void GtkKeyBindingsHandler::HandlerInit(Handler* self) {
  self->owner = nullptr;
}

void GtkKeyBindingsHandler::HandlerClassInit(HandlerClass* klass) {
  GtkTextViewClass* text_view_class = GTK_TEXT_VIEW_CLASS(klass);

  // Overrides all virtual methods related to editor key bindings.
  text_view_class->backspace = BackSpace;
  text_view_class->copy_clipboard = CopyClipboard;
  text_view_class->cut_clipboard = CutClipboard;
  text_view_class->delete_from_cursor = DeleteFromCursor;
  text_view_class->insert_at_cursor = InsertAtCursor;
  text_view_class->move_cursor = MoveCursor;
  text_view_class->paste_clipboard = PasteClipboard;
  text_view_class->set_anchor = SetAnchor;
  text_view_class->toggle_overwrite = ToggleOverwrite;
#if !GTK_CHECK_VERSION(3, 90, 0)
  GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
  widget_class->show_help = ShowHelp;
#endif

  // "move-focus", "move-viewport", "select-all" and "toggle-cursor-visible"
  // have no corresponding virtual methods. Since glib 2.18 (gtk 2.14),
  // g_signal_override_class_handler() is introduced to override a signal
  // handler.
  g_signal_override_class_handler("move-focus", G_TYPE_FROM_CLASS(klass),
                                  G_CALLBACK(MoveFocus));

  g_signal_override_class_handler("move-viewport", G_TYPE_FROM_CLASS(klass),
                                  G_CALLBACK(MoveViewport));

  g_signal_override_class_handler("select-all", G_TYPE_FROM_CLASS(klass),
                                  G_CALLBACK(SelectAll));

  g_signal_override_class_handler("toggle-cursor-visible",
                                  G_TYPE_FROM_CLASS(klass),
                                  G_CALLBACK(ToggleCursorVisible));
}

GType GtkKeyBindingsHandler::HandlerGetType() {
  static volatile gsize type_id_volatile = 0;
  if (g_once_init_enter(&type_id_volatile)) {
    GType type_id = g_type_register_static_simple(
        GTK_TYPE_TEXT_VIEW, g_intern_static_string("GtkKeyBindingsHandler"),
        sizeof(HandlerClass),
        reinterpret_cast<GClassInitFunc>(HandlerClassInit), sizeof(Handler),
        reinterpret_cast<GInstanceInitFunc>(HandlerInit),
        static_cast<GTypeFlags>(0));
    g_once_init_leave(&type_id_volatile, type_id);
  }
  return type_id_volatile;
}

GtkKeyBindingsHandler* GtkKeyBindingsHandler::GetHandlerOwner(
    GtkTextView* text_view) {
  Handler* handler =
      G_TYPE_CHECK_INSTANCE_CAST(text_view, HandlerGetType(), Handler);
  DCHECK(handler);
  return handler->owner;
}

void GtkKeyBindingsHandler::BackSpace(GtkTextView* text_view) {
  GetHandlerOwner(text_view)->EditCommandMatched(
      TextEditCommand::DELETE_BACKWARD, std::string());
}

void GtkKeyBindingsHandler::CopyClipboard(GtkTextView* text_view) {
  GetHandlerOwner(text_view)->EditCommandMatched(TextEditCommand::COPY,
                                                 std::string());
}

void GtkKeyBindingsHandler::CutClipboard(GtkTextView* text_view) {
  GetHandlerOwner(text_view)->EditCommandMatched(TextEditCommand::CUT,
                                                 std::string());
}

void GtkKeyBindingsHandler::DeleteFromCursor(GtkTextView* text_view,
                                             GtkDeleteType type,
                                             gint count) {
  if (!count)
    return;

  TextEditCommand commands[2] = {
      TextEditCommand::INVALID_COMMAND, TextEditCommand::INVALID_COMMAND,
  };
  switch (type) {
    case GTK_DELETE_CHARS:
      commands[0] = (count > 0 ? TextEditCommand::DELETE_FORWARD
                               : TextEditCommand::DELETE_BACKWARD);
      break;
    case GTK_DELETE_WORD_ENDS:
      commands[0] = (count > 0 ? TextEditCommand::DELETE_WORD_FORWARD
                               : TextEditCommand::DELETE_WORD_BACKWARD);
      break;
    case GTK_DELETE_WORDS:
      if (count > 0) {
        commands[0] = TextEditCommand::MOVE_WORD_FORWARD;
        commands[1] = TextEditCommand::DELETE_WORD_BACKWARD;
      } else {
        commands[0] = TextEditCommand::MOVE_WORD_BACKWARD;
        commands[1] = TextEditCommand::DELETE_WORD_FORWARD;
      }
      break;
    case GTK_DELETE_DISPLAY_LINES:
      commands[0] = TextEditCommand::MOVE_TO_BEGINNING_OF_LINE;
      commands[1] = TextEditCommand::DELETE_TO_END_OF_LINE;
      break;
    case GTK_DELETE_DISPLAY_LINE_ENDS:
      commands[0] = (count > 0 ? TextEditCommand::DELETE_TO_END_OF_LINE
                               : TextEditCommand::DELETE_TO_BEGINNING_OF_LINE);
      break;
    case GTK_DELETE_PARAGRAPH_ENDS:
      commands[0] =
          (count > 0 ? TextEditCommand::DELETE_TO_END_OF_PARAGRAPH
                     : TextEditCommand::DELETE_TO_BEGINNING_OF_PARAGRAPH);
      break;
    case GTK_DELETE_PARAGRAPHS:
      commands[0] = TextEditCommand::MOVE_TO_BEGINNING_OF_PARAGRAPH;
      commands[1] = TextEditCommand::DELETE_TO_END_OF_PARAGRAPH;
      break;
    default:
      // GTK_DELETE_WHITESPACE has no corresponding editor command.
      return;
  }

  GtkKeyBindingsHandler* owner = GetHandlerOwner(text_view);
  if (count < 0)
    count = -count;
  for (; count > 0; --count) {
    for (size_t i = 0; i < base::size(commands); ++i)
      if (commands[i] != TextEditCommand::INVALID_COMMAND)
        owner->EditCommandMatched(commands[i], std::string());
  }
}

void GtkKeyBindingsHandler::InsertAtCursor(GtkTextView* text_view,
                                           const gchar* str) {
  if (str && *str) {
    GetHandlerOwner(text_view)->EditCommandMatched(TextEditCommand::INSERT_TEXT,
                                                   str);
  }
}

void GtkKeyBindingsHandler::MoveCursor(GtkTextView* text_view,
                                       GtkMovementStep step,
                                       gint count,
                                       gboolean extend_selection) {
  if (!count)
    return;

  TextEditCommand command;
  switch (step) {
    case GTK_MOVEMENT_LOGICAL_POSITIONS:
      if (extend_selection) {
        command =
            (count > 0 ? TextEditCommand::MOVE_FORWARD_AND_MODIFY_SELECTION
                       : TextEditCommand::MOVE_BACKWARD_AND_MODIFY_SELECTION);
      } else {
        command = (count > 0 ? TextEditCommand::MOVE_FORWARD
                             : TextEditCommand::MOVE_BACKWARD);
      }
      break;
    case GTK_MOVEMENT_VISUAL_POSITIONS:
      if (extend_selection) {
        command = (count > 0 ? TextEditCommand::MOVE_RIGHT_AND_MODIFY_SELECTION
                             : TextEditCommand::MOVE_LEFT_AND_MODIFY_SELECTION);
      } else {
        command = (count > 0 ? TextEditCommand::MOVE_RIGHT
                             : TextEditCommand::MOVE_LEFT);
      }
      break;
    case GTK_MOVEMENT_WORDS:
      if (extend_selection) {
        command =
            (count > 0 ? TextEditCommand::MOVE_WORD_RIGHT_AND_MODIFY_SELECTION
                       : TextEditCommand::MOVE_WORD_LEFT_AND_MODIFY_SELECTION);
      } else {
        command = (count > 0 ? TextEditCommand::MOVE_WORD_RIGHT
                             : TextEditCommand::MOVE_WORD_LEFT);
      }
      break;
    case GTK_MOVEMENT_DISPLAY_LINES:
      if (extend_selection) {
        command = (count > 0 ? TextEditCommand::MOVE_DOWN_AND_MODIFY_SELECTION
                             : TextEditCommand::MOVE_UP_AND_MODIFY_SELECTION);
      } else {
        command =
            (count > 0 ? TextEditCommand::MOVE_DOWN : TextEditCommand::MOVE_UP);
      }
      break;
    case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
      if (extend_selection) {
        command =
            (count > 0
                 ? TextEditCommand::MOVE_TO_END_OF_LINE_AND_MODIFY_SELECTION
                 : TextEditCommand::
                       MOVE_TO_BEGINNING_OF_LINE_AND_MODIFY_SELECTION);
      } else {
        command = (count > 0 ? TextEditCommand::MOVE_TO_END_OF_LINE
                             : TextEditCommand::MOVE_TO_BEGINNING_OF_LINE);
      }
      break;
    case GTK_MOVEMENT_PARAGRAPH_ENDS:
      if (extend_selection) {
        command =
            (count > 0
                 ? TextEditCommand::
                       MOVE_TO_END_OF_PARAGRAPH_AND_MODIFY_SELECTION
                 : TextEditCommand::
                       MOVE_TO_BEGINNING_OF_PARAGRAPH_AND_MODIFY_SELECTION);
      } else {
        command = (count > 0 ? TextEditCommand::MOVE_TO_END_OF_PARAGRAPH
                             : TextEditCommand::MOVE_TO_BEGINNING_OF_PARAGRAPH);
      }
      break;
    case GTK_MOVEMENT_PAGES:
      if (extend_selection) {
        command =
            (count > 0 ? TextEditCommand::MOVE_PAGE_DOWN_AND_MODIFY_SELECTION
                       : TextEditCommand::MOVE_PAGE_UP_AND_MODIFY_SELECTION);
      } else {
        command = (count > 0 ? TextEditCommand::MOVE_PAGE_DOWN
                             : TextEditCommand::MOVE_PAGE_UP);
      }
      break;
    case GTK_MOVEMENT_BUFFER_ENDS:
      if (extend_selection) {
        command =
            (count > 0
                 ? TextEditCommand::MOVE_TO_END_OF_DOCUMENT_AND_MODIFY_SELECTION
                 : TextEditCommand::
                       MOVE_TO_BEGINNING_OF_DOCUMENT_AND_MODIFY_SELECTION);
      } else {
        command = (count > 0 ? TextEditCommand::MOVE_TO_END_OF_DOCUMENT
                             : TextEditCommand::MOVE_TO_BEGINNING_OF_DOCUMENT);
      }
      break;
    default:
      // GTK_MOVEMENT_PARAGRAPHS and GTK_MOVEMENT_HORIZONTAL_PAGES have
      // no corresponding editor commands.
      return;
  }

  GtkKeyBindingsHandler* owner = GetHandlerOwner(text_view);
  if (count < 0)
    count = -count;
  for (; count > 0; --count)
    owner->EditCommandMatched(command, std::string());
}

void GtkKeyBindingsHandler::MoveViewport(GtkTextView* text_view,
                                         GtkScrollStep step,
                                         gint count) {
  // Not supported by Blink.
}

void GtkKeyBindingsHandler::PasteClipboard(GtkTextView* text_view) {
  GetHandlerOwner(text_view)->EditCommandMatched(TextEditCommand::PASTE,
                                                 std::string());
}

void GtkKeyBindingsHandler::SelectAll(GtkTextView* text_view, gboolean select) {
  GetHandlerOwner(text_view)->EditCommandMatched(
      select ? TextEditCommand::SELECT_ALL : TextEditCommand::UNSELECT,
      std::string());
}

void GtkKeyBindingsHandler::SetAnchor(GtkTextView* text_view) {
  GetHandlerOwner(text_view)->EditCommandMatched(TextEditCommand::SET_MARK,
                                                 std::string());
}

void GtkKeyBindingsHandler::ToggleCursorVisible(GtkTextView* text_view) {
  // Not supported by Blink.
}

void GtkKeyBindingsHandler::ToggleOverwrite(GtkTextView* text_view) {
  // Not supported by Blink.
}

#if !GTK_CHECK_VERSION(3, 90, 0)
gboolean GtkKeyBindingsHandler::ShowHelp(GtkWidget* widget,
                                         GtkWidgetHelpType arg1) {
  // Just for disabling the default handler.
  return FALSE;
}
#endif

void GtkKeyBindingsHandler::MoveFocus(GtkWidget* widget,
                                      GtkDirectionType arg1) {
  // Just for disabling the default handler.
}

}  // namespace libgtkui
