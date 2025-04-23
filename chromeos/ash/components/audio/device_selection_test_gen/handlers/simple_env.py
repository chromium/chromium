# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from dsp import BaseHandler, Device, State


class Handler(BaseHandler):
    cache: dict[frozenset, Device]

    def __init__(self):
        self.cache = {}
        self.list = []

    def cache_state(self, after: State) -> State:
        self.cache[after.connected_devices()] = after.get_active()
        return after

    def plug(self, before: State, after: State, device: Device) -> State:
        # G3
        if self.g3_or_first(before, after, device):
            return self.cache_state(after.switch_to(device))

        # follow cache if available
        after_state = after.connected_devices()
        if after_state in self.cache:
            return after.switch_to(self.cache[after_state])

        # follow builtin priority
        if device.type.builtin_priority >= after.get_active().type.builtin_priority:
            return self.cache_state(after.switch_to(device))
        return self.cache_state(after)

    def unplug(self, before: State, after: State, device: Device) -> State:
        # follow cache if available
        after_state = after.connected_devices()
        if after_state in self.cache:
            return after.switch_to(self.cache[after_state])

        # do nothing if active device is not unplugged
        if after.num_active_devices():
            return after

        # auto-select random device with highest priority
        return after.switch_to(after.max_builtin_priority_device())

    def select(self, before: State, device: Device) -> State:
        return self.cache_state(before.switch_to(device))

    def internal_state(self):
        return '; '.join(
            ''.join(sorted(dev.name for dev in k)) + '=' + v.name
            for (k, v) in self.cache.items()
        )
