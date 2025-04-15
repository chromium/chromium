# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from dsp import BaseHandler, Device, State

class Handler(BaseHandler):
    def __init__(self) -> None:
        self.list = []

    def internal_state(self):
        return ' > '.join(dev.name for dev in self.list)

    def switch_to(self, state: State, s: Device) -> State:
        if not self.list:
            self.list = [s]
            return state.switch_to(s)

        e_index = None
        s_index = None
        for i, device in enumerate(self.list):
            if state.devices[device].connected:
                e_index = i
            if device == s:
                s_index = i

        if s_index is None:
            self.list.insert(e_index+1, s)
            return state.switch_to(s)

        if s_index == e_index:
            return state.switch_to(s)

        insert_position = e_index
        if s_index > e_index:
            insert_position += 1
        self.list.pop(s_index)
        self.list.insert(insert_position, s)

        return state.switch_to(s)

    def select(self, before: State, device: Device) -> State:
        return self.switch_to(before, device)

    def plug(self, before: State, after: State, p: Device) -> State:
        if self.g3_or_first(before, after, p):
            return self.switch_to(after, p)

        a = after.get_active()
        try:
            p_index = self.list.index(p)
            a_index = self.list.index(a)
        except ValueError:
            if not p.type.builtin_priority >= a.type.builtin_priority:
                return after
        else:
            if not p_index > a_index:
                return after
        return self.switch_to(after, p)

    def unplug(self, before: State, after: State, device: Device) -> State:
        if not before.devices[device].active:
            return after

        for e in reversed(self.list):
            if after.devices[e].connected:
                return self.switch_to(after, e)

        h = after.max_builtin_priority_device()
        return self.switch_to(after, h)
