// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/client_settings.h"
#include "base/logging.h"
#include "components/autofill_assistant/browser/view_layout.pb.h"

namespace {

bool IsValidOverlayImageProto(
    const autofill_assistant::OverlayImageProto& proto) {
  if ((proto.has_image_drawable() || !proto.image_url().empty()) &&
      !proto.has_image_size()) {
    VLOG(1) << __func__ << ": Missing image_size in overlay_image, ignoring";
    return false;
  }

  if (!proto.text().empty() &&
      (!proto.has_text_color() || !proto.has_text_size())) {
    VLOG(1) << __func__
            << ": Missing text_color or text_size in overlay_image, ignoring";
    return false;
  }
  return true;
}

}  // namespace

namespace autofill_assistant {

ClientSettings::ClientSettings() = default;
ClientSettings::~ClientSettings() = default;

void ClientSettings::UpdateFromProto(const ClientSettingsProto& proto) {
  if (proto.has_periodic_script_check_interval_ms()) {
    periodic_script_check_interval =
        base::Milliseconds(proto.periodic_script_check_interval_ms());
  }
  if (proto.has_periodic_element_check_interval_ms()) {
    periodic_element_check_interval =
        base::Milliseconds(proto.periodic_element_check_interval_ms());
  }
  if (proto.has_periodic_script_check_count()) {
    periodic_script_check_count = proto.periodic_script_check_count();
  }
  if (proto.has_element_position_update_interval_ms()) {
    element_position_update_interval =
        base::Milliseconds(proto.element_position_update_interval_ms());
  }
  if (proto.has_short_wait_for_element_deadline_ms()) {
    short_wait_for_element_deadline =
        base::Milliseconds(proto.short_wait_for_element_deadline_ms());
  }
  if (proto.has_box_model_check_interval_ms()) {
    box_model_check_interval =
        base::Milliseconds(proto.box_model_check_interval_ms());
  }
  if (proto.has_box_model_check_count()) {
    box_model_check_count = proto.box_model_check_count();
  }
  if (proto.has_document_ready_check_timeout_ms()) {
    document_ready_check_timeout =
        base::Milliseconds(proto.document_ready_check_timeout_ms());
  }
  if (proto.has_cancel_delay_ms()) {
    cancel_delay = base::Milliseconds(proto.cancel_delay_ms());
  }
  if (proto.has_tap_count()) {
    tap_count = proto.tap_count();
  }
  if (proto.has_tap_tracking_duration_ms()) {
    tap_tracking_duration =
        base::Milliseconds(proto.tap_tracking_duration_ms());
  }
  if (proto.has_tap_shutdown_delay_ms()) {
    tap_shutdown_delay = base::Milliseconds(proto.tap_shutdown_delay_ms());
  }
  if (proto.has_overlay_image()) {
    // TODO(b/170202574): Add integration test and remove legacy |image_url|.
    if (IsValidOverlayImageProto(proto.overlay_image())) {
      overlay_image = proto.overlay_image();
      // Legacy treatment for |image_url|.
      if (!overlay_image->image_url().empty()) {
        std::string url = overlay_image->image_url();
        auto* bitmap_proto =
            overlay_image->mutable_image_drawable()->mutable_bitmap();
        bitmap_proto->set_url(url);
        *bitmap_proto->mutable_width() = overlay_image->image_size();
        *bitmap_proto->mutable_height() = overlay_image->image_size();
      }
    } else {
      overlay_image.reset();
    }
  }
  if (proto.has_talkback_sheet_size_fraction()) {
    talkback_sheet_size_fraction = proto.talkback_sheet_size_fraction();
  }
  if (proto.has_back_button_settings()) {
    if (proto.back_button_settings().has_undo_label()) {
      back_button_settings = proto.back_button_settings();
    } else {
      back_button_settings.reset();
    }
  }
  if (proto.has_slow_warning_settings()) {
    if (proto.slow_warning_settings().has_enable_slow_connection_warnings()) {
      enable_slow_connection_warnings =
          proto.slow_warning_settings().enable_slow_connection_warnings();
    }
    if (proto.slow_warning_settings().has_enable_slow_website_warnings()) {
      enable_slow_website_warnings =
          proto.slow_warning_settings().enable_slow_website_warnings();
    }
    if (proto.slow_warning_settings().has_only_show_warning_once()) {
      only_show_warning_once =
          proto.slow_warning_settings().only_show_warning_once();
    }
    if (proto.slow_warning_settings().has_only_show_connection_warning_once()) {
      only_show_connection_warning_once =
          proto.slow_warning_settings().only_show_connection_warning_once();
    }
    if (proto.slow_warning_settings().has_only_show_website_warning_once()) {
      only_show_website_warning_once =
          proto.slow_warning_settings().only_show_website_warning_once();
    }
    if (proto.slow_warning_settings().has_warning_delay_ms()) {
      warning_delay =
          base::Milliseconds(proto.slow_warning_settings().warning_delay_ms());
    }
    if (proto.slow_warning_settings().has_slow_roundtrip_threshold_ms()) {
      slow_roundtrip_threshold = base::Milliseconds(
          proto.slow_warning_settings().slow_roundtrip_threshold_ms());
    }
    if (proto.slow_warning_settings().has_max_consecutive_slow_roundtrips()) {
      max_consecutive_slow_roundtrips =
          proto.slow_warning_settings().max_consecutive_slow_roundtrips();
    }
    if (proto.slow_warning_settings().has_slow_connection_message()) {
      slow_connection_message =
          proto.slow_warning_settings().slow_connection_message();
    }
    if (proto.slow_warning_settings().has_slow_website_message()) {
      slow_website_message =
          proto.slow_warning_settings().slow_website_message();
    }
    if (proto.slow_warning_settings()
            .has_minimum_warning_message_duration_ms()) {
      minimum_warning_duration = base::Milliseconds(
          proto.slow_warning_settings().minimum_warning_message_duration_ms());
    }
    if (proto.slow_warning_settings().message_mode() !=
        ClientSettingsProto::SlowWarningSettings::UNKNOWN) {
      message_mode = proto.slow_warning_settings().message_mode();
    }
  }
  if (!proto.display_strings_locale().empty()) {
    if (display_strings_locale != proto.display_strings_locale()) {
      display_strings.clear();
    }
    display_strings_locale = proto.display_strings_locale();
    for (const ClientSettingsProto::DisplayString& display_string :
         proto.display_strings()) {
      display_strings[display_string.id()] = display_string.value();
    }
  } else if (!proto.display_strings().empty()) {
    VLOG(1) << "Rejecting new display strings: no locale provided";
  }
  // Test only settings.
  if (proto.has_integration_test_settings()) {
    integration_test_settings = proto.integration_test_settings();
  } else {
    integration_test_settings.reset();
  }
  if (proto.has_selector_observer_extra_timeout_ms()) {
    selector_observer_extra_timeout =
        base::Milliseconds(proto.selector_observer_extra_timeout_ms());
  }
  if (proto.has_selector_observer_debounce_interval_ms()) {
    selector_observer_debounce_interval =
        base::Milliseconds(proto.selector_observer_debounce_interval_ms());
  }
}

}  // namespace autofill_assistant
