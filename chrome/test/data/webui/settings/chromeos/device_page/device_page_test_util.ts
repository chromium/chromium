// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Utils for device page browser tests. */

export function getFakePrefs() {
  return {
    arc: {
      enabled: {
        key: 'arc.enabled',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
    },
    ash: {
      ambient_color: {
        enabled: {
          key: 'ash.ambient_color.enabled',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
      },
      night_light: {
        enabled: {
          key: 'ash.night_light.enabled',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        color_temperature: {
          key: 'ash.night_light.color_temperature',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 0,
        },
        schedule_type: {
          key: 'ash.night_light.schedule_type',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 0,
        },
        custom_start_time: {
          key: 'ash.night_light.custom_start_time',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 0,
        },
        custom_end_time: {
          key: 'ash.night_light.custom_end_time',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 0,
        },
      },
    },
    gdata: {
      disabled: {
        key: 'gdata.disabled',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
    },
    power: {
      cros_battery_saver_active: {
        key: 'power.cros_battery_saver_active',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
    },
    settings: {
      // TODO(afakhry): Write tests to validate the Night Light slider
      // behavior with 24-hour setting.
      clock: {
        use_24hour_clock: {
          key: 'settings.clock.use_24hour_clock',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
      },
      enable_stylus_tools: {
        key: 'settings.enable_stylus_tools',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
      launch_palette_on_eject_event: {
        key: 'settings.launch_palette_on_eject_event',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
      restore_last_lock_screen_note: {
        key: 'settings.restore_last_lock_screen_note',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
      touchpad: {
        enable_tap_to_click: {
          key: 'settings.touchpad.enable_tap_to_click',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        enable_tap_dragging: {
          key: 'settings.touchpad.enable_tap_dragging',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        natural_scroll: {
          key: 'settings.touchpad.natural_scroll',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        acceleration: {
          key: 'settings.touchpad.acceleration',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        scroll_acceleration: {
          key: 'settings.touchpad.scroll_acceleration',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        sensitivity2: {
          key: 'settings.touchpad.sensitivity2',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 3,
        },
        scroll_sensitivity: {
          key: 'settings.touchpad.scroll_sensitivity',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 3,
        },
        haptic_feedback: {
          key: 'settings.touchpad.haptic_feedback',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        haptic_click_sensitivity: {
          key: 'settings.touchpad.haptic_click_sensitivity',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 3,
        },
      },
      mouse: {
        primary_right: {
          key: 'settings.mouse.primary_right',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        reverse_scroll: {
          key: 'settings.mouse.reverse_scroll',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        acceleration: {
          key: 'settings.mouse.acceleration',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        scroll_acceleration: {
          key: 'settings.mouse.scroll_acceleration',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        sensitivity2: {
          key: 'settings.mouse.sensitivity2',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 4,
        },
        scroll_sensitivity: {
          key: 'settings.mouse.scroll_sensitivity',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 4,
        },
      },
      pointing_stick: {
        primary_right: {
          key: 'settings.pointing_stick.primary_right',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        acceleration: {
          key: 'settings.pointing_stick.acceleration',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        sensitivity: {
          key: 'settings.pointing_stick.sensitivity',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 4,
        },
      },
      language: {
        xkb_remap_search_key_to: {
          key: 'settings.language.xkb_remap_search_key_to',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 0,
        },
        xkb_remap_control_key_to: {
          key: 'settings.language.xkb_remap_control_key_to',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 1,
        },
        xkb_remap_alt_key_to: {
          key: 'settings.language.xkb_remap_alt_key_to',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 2,
        },
        remap_caps_lock_key_to: {
          key: 'settings.language.remap_caps_lock_key_to',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 4,
        },
        remap_escape_key_to: {
          key: 'settings.language.remap_escape_key_to',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 5,
        },
        remap_backspace_key_to: {
          key: 'settings.language.remap_backspace_key_to',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 6,
        },
        send_function_keys: {
          key: 'settings.language.send_function_keys',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        xkb_auto_repeat_enabled_r2: {
          key: 'prefs.settings.language.xkb_auto_repeat_enabled_r2',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        xkb_auto_repeat_delay_r2: {
          key: 'settings.language.xkb_auto_repeat_delay_r2',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 500,
        },
        xkb_auto_repeat_interval_r2: {
          key: 'settings.language.xkb_auto_repeat_interval_r2',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 500,
        },
      },
    },
  };
}
