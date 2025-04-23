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
        active = after.get_active()
        assert active.type.stationary
        self.cache[after.connected_stationary_devices()] = active
        return after

    def plug(self, before: State, after: State, device: Device) -> State:
        # G3
        if self.g3_or_first(before, after, device):
            return after.switch_to(device)

        if not device.type.stationary:
            return after.switch_to(device)

        # follow cache if available
        after_state = after.connected_stationary_devices()
        if after_state in self.cache:
            return after.switch_to(self.cache[after_state])

        # use plugged device
        return self.cache_state(after.switch_to(device))

    def unplug(self, before: State, after: State, device: Device) -> State:
        # do nothing if active device is not unplugged
        if after.num_active_devices():
            return after

        if not device.type.stationary:
            portables = after.connected_portable_devices()
            if not portables:
                if after.connected_stationary_devices() in self.cache:
                    return after.switch_to(self.cache[after.connected_stationary_devices()])
            return after.switch_to(after.max_builtin_priority_device())

        if after.connected_stationary_devices() in self.cache:
            return after.switch_to(self.cache[after.connected_stationary_devices()])

        # auto-select random device with highest priority
        return after.switch_to(after.max_builtin_priority_device())

    def select(self, before: State, device: Device) -> State:
        if device.type.stationary:
            return self.cache_state(before.switch_to(device))
        return before.switch_to(device)

    def internal_state(self):
        return '; '.join(
            ''.join(sorted(dev.name for dev in k)) + '=' + v.name
            for (k, v) in self.cache.items()
        )
