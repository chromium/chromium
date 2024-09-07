// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/chromecast/events/starboard_event_source.h"

#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace chromecast {

namespace {

constexpr char kPropertyFromStarboard[] = "from_sb";
constexpr size_t kPropertyFromStarboardSize = 1;

// This map represents the key codes that can be recognized by Cast. The
// resulting DomCodes are the union of:
// - Keys which are used by the Cast SDK.
// - Keys which are used by the Cast SDK when the DPAD UI is enabled.
// - Keys which are not used by the Cast SDK, but are defined in the HDMI CEC
//   specification and may be useful to apps.
constexpr auto kSbKeyToDomCodeMap = base::MakeFixedFlatMap<SbKey, ui::DomCode>({
    // Convenience keys for keyboard support.
    {kSbKeySpace, ui::DomCode::MEDIA_PLAY_PAUSE},

    // Keys which are used by the Cast SDK.
    {kSbKeyReturn, ui::DomCode::ENTER},
    {kSbKeySelect, ui::DomCode::SELECT},
    {kSbKeyUp, ui::DomCode::ARROW_UP},
    {kSbKeyDown, ui::DomCode::ARROW_DOWN},
    {kSbKeyLeft, ui::DomCode::ARROW_LEFT},
    {kSbKeyRight, ui::DomCode::ARROW_RIGHT},
    {kSbKeyBack, ui::DomCode::BROWSER_BACK},

    // Keys which are used by the Cast SDK when the DPAD UI is enabled.
    {kSbKeyMediaPlayPause, ui::DomCode::MEDIA_PLAY_PAUSE},
    {kSbKeyMediaRewind, ui::DomCode::MEDIA_REWIND},
    {kSbKeyMediaFastForward, ui::DomCode::MEDIA_FAST_FORWARD},
    {kSbKeyMediaNextTrack, ui::DomCode::MEDIA_TRACK_NEXT},
    {kSbKeyMediaPrevTrack, ui::DomCode::MEDIA_TRACK_PREVIOUS},
    {kSbKeyPause, ui::DomCode::MEDIA_PAUSE},
    {kSbKeyPlay, ui::DomCode::MEDIA_PLAY},
    {kSbKeyMediaStop, ui::DomCode::MEDIA_STOP},

    // Keys which are not used by the Cast SDK, but are defined in the HDMI CEC
    // specification.
    {kSbKeyMenu, ui::DomCode::HOME},
    {kSbKeyChannelUp, ui::DomCode::CHANNEL_UP},
    {kSbKeyChannelDown, ui::DomCode::CHANNEL_DOWN},
    {kSbKeyClosedCaption, ui::DomCode::CLOSED_CAPTION_TOGGLE},
#if SB_API_VERSION >= 15
    {kSbKeyRecord, ui::DomCode::MEDIA_RECORD},
#endif  // SB_API_VERSION >=15
});

// Returns the current SequencedTaskRunner. Crashes if not called from a
// sequenced task runner.
scoped_refptr<base::SequencedTaskRunner> GetCurrentSequencedTaskRunner() {
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  return base::SequencedTaskRunner::GetCurrentDefault();
}

}  // namespace

// static
void StarboardEventSource::SbEventHandle(void* context, const SbEvent* event) {
  reinterpret_cast<StarboardEventSource*>(context)->SbEventHandleInternal(
      event);
}

void StarboardEventSource::SbEventHandleInternal(const SbEvent* event) {
  if (event->type != kSbEventTypeInput) {
    return;
  }

  if (event->data == nullptr) {
    return;
  }
  auto* input_data = static_cast<SbInputData*>(event->data);

  SbTimeMonotonic raw_timestamp = event->timestamp;
  SbInputEventType raw_type = input_data->type;
  SbKey raw_key = input_data->key;
  if (raw_type != kSbInputEventTypePress &&
      raw_type != kSbInputEventTypeUnpress) {
    return;
  }

  // Find out if the press is supported by Cast.
  auto it = kSbKeyToDomCodeMap.find(raw_key);
  if (it == kSbKeyToDomCodeMap.end()) {
    return;
  }

  std::unique_ptr<ui::Event> ui_event;
  ui::DomKey dom_key;
  ui::KeyboardCode key_code;
  ui::DomCode dom_code = it->second;
  int flags = 0;
  if (!DomCodeToUsLayoutDomKey(dom_code, flags, &dom_key, &key_code)) {
    return;
  }

  // Key press.
  ui::EventType event_type = raw_type == kSbInputEventTypePress
                                 ? ui::EventType::kKeyPressed
                                 : ui::EventType::kKeyReleased;
  ui_event = std::make_unique<ui::KeyEvent>(
      event_type, key_code, dom_code, flags, dom_key,
      /*time_stamp=*/
      base::TimeTicks() + base::Microseconds(raw_timestamp));

  ui::Event::Properties properties;
  properties[chromecast::kPropertyFromStarboard] =
      std::vector<uint8_t>(chromecast::kPropertyFromStarboardSize);
  ui_event->SetProperties(properties);
  DispatchUiEvent(std::move(ui_event));
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
  CastStarboardApiAdapter::GetInstance()->Subscribe(
      this, &StarboardEventSource::SbEventHandle);
}

StarboardEventSource::~StarboardEventSource() {
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
