// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/chromecast/events/starboard_event_source.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace chromecast {

namespace {

constexpr char kPropertyFromStarboard[] = "from_sb";
constexpr size_t kPropertyFromStarboardSize = 1;

const base::flat_map<SbKey, ui::DomCode>& GetSbKeyToDomCodeMap() {
  static const base::NoDestructor<base::flat_map<SbKey, ui::DomCode>> kMap({
      // Common Keys
      {kSbKeySpace, ui::DomCode::SPACE},
      {kSbKeyReturn, ui::DomCode::ENTER},
      {kSbKeySelect, ui::DomCode::SELECT},
      {kSbKeyUp, ui::DomCode::ARROW_UP},
      {kSbKeyDown, ui::DomCode::ARROW_DOWN},
      {kSbKeyLeft, ui::DomCode::ARROW_LEFT},
      {kSbKeyRight, ui::DomCode::ARROW_RIGHT},
      {kSbKeyBack, ui::DomCode::BROWSER_BACK},
      {kSbKeyEscape, ui::DomCode::ESCAPE},
      {kSbKeyTab, ui::DomCode::TAB},
      {kSbKeyBacktab,
       ui::DomCode::TAB},  // No distinct backtab in DomCode, handle with Shift
      {kSbKeyOem1, ui::DomCode::SEMICOLON},
      {kSbKeyOemPlus, ui::DomCode::EQUAL},
      {kSbKeyOemComma, ui::DomCode::COMMA},
      {kSbKeyOemMinus, ui::DomCode::MINUS},
      {kSbKeyOemPeriod, ui::DomCode::PERIOD},
      {kSbKeyOem2, ui::DomCode::SLASH},
      {kSbKeyOem3, ui::DomCode::BACKQUOTE},
      {kSbKeyOem4, ui::DomCode::BRACKET_LEFT},
      {kSbKeyOem5, ui::DomCode::BACKSLASH},
      {kSbKeyOem6, ui::DomCode::BRACKET_RIGHT},
      {kSbKeyOem7, ui::DomCode::QUOTE},

      // Media Keys
      {kSbKeyMediaPlayPause, ui::DomCode::MEDIA_PLAY_PAUSE},
      {kSbKeyMediaRewind, ui::DomCode::MEDIA_REWIND},
      {kSbKeyMediaFastForward, ui::DomCode::MEDIA_FAST_FORWARD},
      {kSbKeyMediaNextTrack, ui::DomCode::MEDIA_TRACK_NEXT},
      {kSbKeyMediaPrevTrack, ui::DomCode::MEDIA_TRACK_PREVIOUS},
      {kSbKeyPause, ui::DomCode::PAUSE},
      {kSbKeyPlay, ui::DomCode::MEDIA_PLAY},
      {kSbKeyMediaStop, ui::DomCode::MEDIA_STOP},
      {kSbKeyChannelUp, ui::DomCode::CHANNEL_UP},
      {kSbKeyChannelDown, ui::DomCode::CHANNEL_DOWN},
      {kSbKeyClosedCaption, ui::DomCode::CLOSED_CAPTION_TOGGLE},
#if SB_API_VERSION >= 15
      {kSbKeyRecord, ui::DomCode::MEDIA_RECORD},
#endif  // SB_API_VERSION >=15
      {kSbKeyVolumeUp, ui::DomCode::VOLUME_UP},
      {kSbKeyVolumeDown, ui::DomCode::VOLUME_DOWN},
      {kSbKeyVolumeMute, ui::DomCode::VOLUME_MUTE},

      // Alphabet
      {kSbKeyA, ui::DomCode::US_A},
      {kSbKeyB, ui::DomCode::US_B},
      {kSbKeyC, ui::DomCode::US_C},
      {kSbKeyD, ui::DomCode::US_D},
      {kSbKeyE, ui::DomCode::US_E},
      {kSbKeyF, ui::DomCode::US_F},
      {kSbKeyG, ui::DomCode::US_G},
      {kSbKeyH, ui::DomCode::US_H},
      {kSbKeyI, ui::DomCode::US_I},
      {kSbKeyJ, ui::DomCode::US_J},
      {kSbKeyK, ui::DomCode::US_K},
      {kSbKeyL, ui::DomCode::US_L},
      {kSbKeyM, ui::DomCode::US_M},
      {kSbKeyN, ui::DomCode::US_N},
      {kSbKeyO, ui::DomCode::US_O},
      {kSbKeyP, ui::DomCode::US_P},
      {kSbKeyQ, ui::DomCode::US_Q},
      {kSbKeyR, ui::DomCode::US_R},
      {kSbKeyS, ui::DomCode::US_S},
      {kSbKeyT, ui::DomCode::US_T},
      {kSbKeyU, ui::DomCode::US_U},
      {kSbKeyV, ui::DomCode::US_V},
      {kSbKeyW, ui::DomCode::US_W},
      {kSbKeyX, ui::DomCode::US_X},
      {kSbKeyY, ui::DomCode::US_Y},
      {kSbKeyZ, ui::DomCode::US_Z},

      // Digits
      {kSbKey0, ui::DomCode::DIGIT0},
      {kSbKey1, ui::DomCode::DIGIT1},
      {kSbKey2, ui::DomCode::DIGIT2},
      {kSbKey3, ui::DomCode::DIGIT3},
      {kSbKey4, ui::DomCode::DIGIT4},
      {kSbKey5, ui::DomCode::DIGIT5},
      {kSbKey6, ui::DomCode::DIGIT6},
      {kSbKey7, ui::DomCode::DIGIT7},
      {kSbKey8, ui::DomCode::DIGIT8},
      {kSbKey9, ui::DomCode::DIGIT9},

      // Numpad Digits
      {kSbKeyNumpad0, ui::DomCode::NUMPAD0},
      {kSbKeyNumpad1, ui::DomCode::NUMPAD1},
      {kSbKeyNumpad2, ui::DomCode::NUMPAD2},
      {kSbKeyNumpad3, ui::DomCode::NUMPAD3},
      {kSbKeyNumpad4, ui::DomCode::NUMPAD4},
      {kSbKeyNumpad5, ui::DomCode::NUMPAD5},
      {kSbKeyNumpad6, ui::DomCode::NUMPAD6},
      {kSbKeyNumpad7, ui::DomCode::NUMPAD7},
      {kSbKeyNumpad8, ui::DomCode::NUMPAD8},
      {kSbKeyNumpad9, ui::DomCode::NUMPAD9},

      // Numpad Others
      {kSbKeyMultiply, ui::DomCode::NUMPAD_MULTIPLY},
      {kSbKeyAdd, ui::DomCode::NUMPAD_ADD},
      {kSbKeySubtract, ui::DomCode::NUMPAD_SUBTRACT},
      {kSbKeyDecimal, ui::DomCode::NUMPAD_DECIMAL},
      {kSbKeyDivide, ui::DomCode::NUMPAD_DIVIDE},
      {kSbKeyNumlock, ui::DomCode::NUM_LOCK},

      // Function Keys
      {kSbKeyF1, ui::DomCode::F1},
      {kSbKeyF2, ui::DomCode::F2},
      {kSbKeyF3, ui::DomCode::F3},
      {kSbKeyF4, ui::DomCode::F4},
      {kSbKeyF5, ui::DomCode::F5},
      {kSbKeyF6, ui::DomCode::F6},
      {kSbKeyF7, ui::DomCode::F7},
      {kSbKeyF8, ui::DomCode::F8},
      {kSbKeyF9, ui::DomCode::F9},
      {kSbKeyF10, ui::DomCode::F10},
      {kSbKeyF11, ui::DomCode::F11},
      {kSbKeyF12, ui::DomCode::F12},

      // Modifiers
      {kSbKeyShift, ui::DomCode::SHIFT_LEFT},
      {kSbKeyLshift, ui::DomCode::SHIFT_LEFT},
      {kSbKeyRshift, ui::DomCode::SHIFT_RIGHT},
      {kSbKeyControl, ui::DomCode::CONTROL_LEFT},
      {kSbKeyLcontrol, ui::DomCode::CONTROL_LEFT},
      {kSbKeyRcontrol, ui::DomCode::CONTROL_RIGHT},
      {kSbKeyMenu, ui::DomCode::ALT_LEFT},
      {kSbKeyLmenu, ui::DomCode::ALT_LEFT},
      {kSbKeyRmenu, ui::DomCode::ALT_RIGHT},
      {kSbKeyLwin, ui::DomCode::META_LEFT},
      {kSbKeyRwin, ui::DomCode::META_RIGHT},
      {kSbKeyApps, ui::DomCode::CONTEXT_MENU},

      // Other common keys
      {kSbKeyCapital, ui::DomCode::CAPS_LOCK},
      {kSbKeyBackspace, ui::DomCode::BACKSPACE},
      {kSbKeyDelete, ui::DomCode::DEL},
      {kSbKeyInsert, ui::DomCode::INSERT},
      {kSbKeyHome, ui::DomCode::HOME},
      {kSbKeyEnd, ui::DomCode::END},
      {kSbKeyPrior, ui::DomCode::PAGE_UP},
      {kSbKeyNext, ui::DomCode::PAGE_DOWN},
      {kSbKeyPrint, ui::DomCode::PRINT_SCREEN},
      {kSbKeySnapshot, ui::DomCode::PRINT_SCREEN},
      {kSbKeyScroll, ui::DomCode::SCROLL_LOCK},

      // Browser Keys
      {kSbKeyBrowserBack, ui::DomCode::BROWSER_BACK},
      {kSbKeyBrowserForward, ui::DomCode::BROWSER_FORWARD},
      {kSbKeyBrowserRefresh, ui::DomCode::BROWSER_REFRESH},
      {kSbKeyBrowserStop, ui::DomCode::BROWSER_STOP},
      {kSbKeyBrowserSearch, ui::DomCode::BROWSER_SEARCH},
      {kSbKeyBrowserFavorites, ui::DomCode::BROWSER_FAVORITES},
      {kSbKeyBrowserHome, ui::DomCode::BROWSER_HOME},
  });
  return *kMap;
}

// Helper function to map Starboard modifiers to ui::EventFlags
int MapStarboardModifiersToUiFlags(unsigned int sb_modifiers) {
  int ui_flags = 0;
  if (sb_modifiers & kSbKeyModifiersAlt) {
    ui_flags |= ui::EF_ALT_DOWN;
  }
  if (sb_modifiers & kSbKeyModifiersCtrl) {
    ui_flags |= ui::EF_CONTROL_DOWN;
  }
  if (sb_modifiers & kSbKeyModifiersMeta) {
    ui_flags |= ui::EF_COMMAND_DOWN;
  }
  if (sb_modifiers & kSbKeyModifiersShift) {
    ui_flags |= ui::EF_SHIFT_DOWN;
  }

  if (sb_modifiers & kSbKeyModifiersPointerButtonLeft) {
    ui_flags |= ui::EF_LEFT_MOUSE_BUTTON;
  }
  if (sb_modifiers & kSbKeyModifiersPointerButtonRight) {
    ui_flags |= ui::EF_RIGHT_MOUSE_BUTTON;
  }
  if (sb_modifiers & kSbKeyModifiersPointerButtonMiddle) {
    ui_flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
  }
  if (sb_modifiers & kSbKeyModifiersPointerButtonBack) {
    ui_flags |= ui::EF_BACK_MOUSE_BUTTON;
  }
  if (sb_modifiers & kSbKeyModifiersPointerButtonForward) {
    ui_flags |= ui::EF_FORWARD_MOUSE_BUTTON;
  }
  return ui_flags;
}

// Helper to create Touch events
std::unique_ptr<ui::Event> CreateTouchEvent(const SbInputData* input_data,
                                            base::TimeTicks timestamp,
                                            int ui_flags) {
  const SbInputEventType raw_type = input_data->type;
  const gfx::PointF position(input_data->position.x, input_data->position.y);

  ui::EventType event_type;
  switch (raw_type) {
    case kSbInputEventTypePress:
      event_type = ui::EventType::kTouchPressed;
      break;
    case kSbInputEventTypeUnpress:
      event_type = ui::EventType::kTouchReleased;
      break;
    case kSbInputEventTypeMove:
      event_type = ui::EventType::kTouchMoved;
      break;
    default:
      VLOG(1) << "Unhandled Touch EventType: " << raw_type;
      return nullptr;
  }

  // SbInputData currently only supports single touch, so pointer_id is
  // hardcoded to 0.
  ui::PointerDetails pointer_details(ui::EventPointerType::kTouch,
                                     /*pointer_id=*/0,
                                     /*radius_x=*/input_data->size.x / 2.0f,
                                     /*radius_y=*/input_data->size.y / 2.0f,
                                     /*force=*/input_data->pressure);

  if (!std::isnan(input_data->tilt.x) && !std::isnan(input_data->tilt.y)) {
    pointer_details.tilt_x = input_data->tilt.x;
    pointer_details.tilt_y = input_data->tilt.y;
  }

  return std::make_unique<ui::TouchEvent>(event_type, position, position,
                                          timestamp, pointer_details, ui_flags);
}

// Helper to create Key events
std::unique_ptr<ui::Event> CreateKeyEvent(const SbInputData* input_data,
                                          base::TimeTicks timestamp,
                                          int ui_flags) {
  const SbInputEventType raw_type = input_data->type;
  const SbKey raw_key = input_data->key;

  if (raw_type != kSbInputEventTypePress &&
      raw_type != kSbInputEventTypeUnpress) {
    return nullptr;
  }

  const auto& key_map = GetSbKeyToDomCodeMap();
  auto it = key_map.find(raw_key);
  if (it != key_map.end()) {
    ui::DomCode dom_code = it->second;
    ui::DomKey dom_key;
    ui::KeyboardCode key_code;
    int current_flags = ui_flags;
    if (raw_key == kSbKeyBacktab) {
      current_flags |= ui::EF_SHIFT_DOWN;
    }

    if (ui::DomCodeToUsLayoutDomKey(dom_code, current_flags, &dom_key,
                                    &key_code)) {
      ui::EventType event_type = (raw_type == kSbInputEventTypePress)
                                     ? ui::EventType::kKeyPressed
                                     : ui::EventType::kKeyReleased;
      return std::make_unique<ui::KeyEvent>(event_type, key_code, dom_code,
                                            current_flags, dom_key, timestamp);
    }
  } else {
    VLOG(1) << "Unhandled SbKey: " << raw_key;
  }
  return nullptr;
}

scoped_refptr<base::SequencedTaskRunner> GetCurrentSequencedTaskRunner() {
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  return base::SequencedTaskRunner::GetCurrentDefault();
}

}  // namespace

// Helper to create Mouse events
std::unique_ptr<ui::Event> StarboardEventSource::CreateMouseEvent(
    const SbInputData* input_data,
    base::TimeTicks timestamp,
    int ui_flags) {
  const SbInputEventType raw_type = input_data->type;
  const SbKey raw_key = input_data->key;
  const gfx::PointF position(input_data->position.x, input_data->position.y);
  switch (raw_type) {
    case kSbInputEventTypePress:
    case kSbInputEventTypeUnpress: {
      int changed_button_flag = 0;
      switch (raw_key) {
        case kSbKeyMouse1:
          changed_button_flag = ui::EF_LEFT_MOUSE_BUTTON;
          break;
        case kSbKeyMouse2:
          changed_button_flag = ui::EF_RIGHT_MOUSE_BUTTON;
          break;
        case kSbKeyMouse3:
          changed_button_flag = ui::EF_MIDDLE_MOUSE_BUTTON;
          break;
        case kSbKeyMouse4:
          changed_button_flag = ui::EF_BACK_MOUSE_BUTTON;
          break;
        case kSbKeyMouse5:
          changed_button_flag = ui::EF_FORWARD_MOUSE_BUTTON;
          break;
        default:
          return nullptr;  // Not a mouse button key
      }

      // Starboard doesn't have a concept of dragging, which is conventionally
      // required by the platform. Simulate it by tracking mouse presses.
      // ui::EventFlags are used because they are bit flags.
      key_flags_ ^= changed_button_flag;
      ui::EventType event_type = (raw_type == kSbInputEventTypePress)
                                     ? ui::EventType::kMousePressed
                                     : ui::EventType::kMouseReleased;
      return std::make_unique<ui::MouseEvent>(
          event_type, position, position, timestamp,
          ui_flags | key_flags_ | changed_button_flag, changed_button_flag);
    }
    case kSbInputEventTypeMove: {
      ui::EventType event_type = key_flags_ ? ui::EventType::kMouseDragged
                                            : ui::EventType::kMouseMoved;
      return std::make_unique<ui::MouseEvent>(
          event_type, position, position, timestamp, ui_flags | key_flags_, 0);
    }
    case kSbInputEventTypeWheel: {
      // Standard multiplier for converting line deltas to scroll offsets.
      const int kScrollOffsetMultiplier = ui::MouseWheelEvent::kWheelDelta;
      int offset_x =
          static_cast<int>(-input_data->delta.x * kScrollOffsetMultiplier);
      int offset_y =
          static_cast<int>(-input_data->delta.y * kScrollOffsetMultiplier);

      return std::make_unique<ui::MouseWheelEvent>(
          gfx::Vector2d(offset_x, offset_y), position, position, timestamp,
          ui_flags | key_flags_, ui::EF_NONE);
    }
    default:
      VLOG(1) << "Unhandled Mouse EventType: " << raw_type;
      return nullptr;
  }
}

// static
void StarboardEventSource::SbEventHandle(void* context, const SbEvent* event) {
  reinterpret_cast<StarboardEventSource*>(context)->SbEventHandleInternal(
      event);
}

void StarboardEventSource::SbEventHandleInternal(const SbEvent* event) {
  if (event->type != kSbEventTypeInput || event->data == nullptr) {
    return;
  }
  const auto* input_data = static_cast<const SbInputData*>(event->data);

  base::TimeTicks timestamp =
      base::TimeTicks() + base::Microseconds(event->timestamp);
  int ui_flags = MapStarboardModifiersToUiFlags(input_data->key_modifiers);

  std::unique_ptr<ui::Event> ui_event;
  switch (input_data->device_type) {
    case kSbInputDeviceTypeMouse:
      ui_event = CreateMouseEvent(input_data, timestamp, ui_flags);
      break;
    case kSbInputDeviceTypeTouchScreen:
      ui_event = CreateTouchEvent(input_data, timestamp, ui_flags);
      break;
    case kSbInputDeviceTypeKeyboard:
    case kSbInputDeviceTypeRemote:
      ui_event = CreateKeyEvent(input_data, timestamp, ui_flags);
      break;
    default:
      VLOG(1) << "Ignoring event from device type: " << input_data->device_type;
      break;
  }

  if (ui_event) {
    ui::Event::Properties properties;
    properties[kPropertyFromStarboard] =
        std::vector<uint8_t>(kPropertyFromStarboardSize);
    ui_event->SetProperties(properties);

    DispatchUiEvent(std::move(ui_event));
  }
}

void StarboardEventSource::DispatchUiEvent(std::unique_ptr<ui::Event> event) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&StarboardEventSource::DispatchUiEvent,
                       weak_factory_.GetWeakPtr(), std::move(event)));
    return;
  }

  delegate_->DispatchEvent(event.get());
}

StarboardEventSource::StarboardEventSource(ui::PlatformWindowDelegate* delegate)
    : task_runner_(GetCurrentSequencedTaskRunner()), delegate_(delegate) {
  DCHECK(delegate_);
  LOG(INFO) << "Subscribing to CastStarboardApiAdapter. this=" << this;
  CastStarboardApiAdapter::GetInstance()->Subscribe(
      this, &StarboardEventSource::SbEventHandle);
}

StarboardEventSource::~StarboardEventSource() {
  LOG(INFO) << "Unsubscribing from CastStarboardApiAdapter. this=" << this;
  CastStarboardApiAdapter::GetInstance()->Unsubscribe(this);
}

bool StarboardEventSource::ShouldDispatchEvent(const ui::Event& event) {
  const ui::Event::Properties* properties = event.properties();
  return properties && properties->find(chromecast::kPropertyFromStarboard) !=
                           properties->end();
}

// Declared in starboard_event_source.h.
std::unique_ptr<UiEventSource> UiEventSource::Create(
    ui::PlatformWindowDelegate* delegate) {
  return std::make_unique<StarboardEventSource>(delegate);
}

}  // namespace chromecast
