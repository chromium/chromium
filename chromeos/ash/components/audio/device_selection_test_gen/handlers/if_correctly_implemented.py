# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from dsp import BaseHandler, Device, State


class Handler(BaseHandler):
    def __init__(self) -> None:
        self.user_activated = set()
        self.known = set()
        self.list = []

    def internal_state(self):
        return (
            'user-activated: '
            + ' '.join(sorted(dev.name for dev in self.user_activated))
            + '; seen: '
            + ' '.join(sorted(dev.name for dev in self.known))
        )

    def switch_to(self, state: State, device: Device) -> State:
        self.user_activated.difference_update(state.devices)
        self.user_activated.add(device)
        return state.switch_to(device)

    def select(self, before: State, device: Device) -> State:
        return self.switch_to(before, device)

    def plug(self, before: State, after: State, device: Device) -> State:
        try:
            if self.g3_or_first(before, after, device):
                return self.switch_to(after, device)

            if device not in self.known:
                switch = (
                    device.type.builtin_priority
                    >= after.get_active().type.builtin_priority
                )
            else:
                switch = device in self.user_activated

            if switch:
                return self.switch_to(after, device)
            return self.switch_to(after, device)
        finally:
            self.known.add(device)

    def unplug(self, before: State, after: State, device: Device) -> State:
        if not before.devices[device].active:
            return after

        h = after.max_builtin_priority_device()
        return self.switch_to(after, h)
