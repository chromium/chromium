# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""
dsp.py - Device Selection Playground
"""

from __future__ import annotations

import builtins
import collections
import dataclasses
import re
import string
import enum
import functools
from typing import ClassVar, Optional


@dataclasses.dataclass(frozen=True)
class Type:
    name: str
    builtin_priority: int
    stationary: bool

    def __repr__(self):
        return f'{self.name}:{self.builtin_priority}'


class T:
    Headphone = Type("Headphone", 4, False)
    USB = Type("USB", 3, False)
    HDMI = Type("HDMI", 2, True)
    Internal = Type("Internal", 1, True)


@dataclasses.dataclass(frozen=True)
class Device:
    name: str
    type: Type

    def __repr__(self) -> str:
        return self.name


@dataclasses.dataclass(frozen=True)
class DeviceState:
    connected: bool = False
    active: bool = False
    connected_at: int = 0

    def __post_init__(self):
        # https://docs.python.org/3/library/dataclasses.html#frozen-instances
        object.__setattr__(
            self, 'connected_at', self.timestamp() if self.connected else 0
        )

    timestamp_counter: ClassVar = 0

    @classmethod
    def timestamp(cls):
        cls.timestamp_counter += 1
        return cls.timestamp_counter


class Observer:
    def add(event: str):
        pass


@dataclasses.dataclass(frozen=True)
class State:
    devices: dict[Device, DeviceState]

    @classmethod
    def with_devices(cls, devices: iter[Device]):
        return cls({dev: DeviceState() for dev in devices})

    def with_mutations(self, mutations: dict[Device, DeviceState]) -> State:
        return State(
            {dev: mutations.get(dev, state) for (dev, state) in self.devices.items()}
        )

    def plug(self, device: Device) -> State:
        assert not self.devices[device].connected
        return self.with_mutations({device: DeviceState(connected=True)})

    def unplug(self, device: Device) -> State:
        assert self.devices[device].connected, (device, self.devices)
        return self.with_mutations({device: DeviceState()})

    def switch_to(self, device: Device) -> State:
        assert self.devices[device].connected, (device, self.devices)
        return State(
            {
                dev: dataclasses.replace(state, active=dev == device)
                for (dev, state) in self.devices.items()
            }
        )

    def active_devices(self):
        for (dev, state) in self.devices.items():
            if state.active:
                yield dev

    def num_active_devices(self):
        return sum(1 for _ in self.active_devices())

    def get_active(self) -> Device:
        (active,) = list(self.active_devices())
        return active

    def connected_devices(self):
        return tuple(
            dev
            for (dev, state) in sorted(self.devices.items(), key=lambda d: d[0].name)
            if state.connected
        )

    def connected_stationary_devices(self):
        return tuple(dev for dev in self.connected_devices() if dev.type.stationary)

    def connected_portable_devices(self):
        return tuple(dev for dev in self.connected_devices() if not dev.type.stationary)

    def max_builtin_priority_device(self, of=None):
        if of is None:
            of = [dev for (dev, state) in self.devices.items() if state.connected]
        return max(
            (dev.type.builtin_priority, self.devices[dev].connected_at, dev)
            for dev in of
        )[2]


class BaseHandler:
    def g3_or_first(
        self, before: State, after: State, device: Device
    ) -> Optional[State]:
        """
        default logic of handling plugs
        """
        if after.num_active_devices() == 0:
            return True
        if device.type == T.Headphone:
            return True
        return False

    def plug(self, before: State, after: State, device: Device) -> State:
        return after

    def unplug(self, before: State, after: State, device: Device) -> State:
        return after

    def select(self, before: State, device: Device) -> State:
        return before.switch_to(device)

    def internal_state(self):
        return "none"


class Simulator:
    def __init__(self, h: BaseHandler, name: str = 'unnamed') -> None:
        self.name = name
        self.devices: list[Device] = []
        self.state = None
        self.h = h
        self.step = 0
        self.visited_states = {}

    def decl(self, name, type):
        assert self.state is None, 'cannot declare devices after start'
        dev = Device(name, type)
        self.devices.append(dev)
        return dev

    def abc(self, *types, charset=string.ascii_uppercase):
        for char, type in zip(charset, types):
            yield self.decl(char, type)
        self.start()

    def start(self):
        self.state = State.with_devices(self.devices)
        print('=' * 40)
        print('test:', self.name)
        print('devices:')
        for dev in self.devices:
            print(
                f'{dev.name} type={dev.type.name} builtin_priority={dev.type.builtin_priority}'
            )

    def next_step(self, event):
        self.step += 1
        print('-' * 40)
        print(f'{self.step}. {event}')

    def plug(self, device):
        self.next_step(f'user plugs {device.name}')
        before = self.state
        after = self.h.plug(before, before.plug(device), device)

        self.print_info(before, after)
        self.check_g1(before, after, device)
        self.check_g3(after, device)
        self.check_g4(after)
        self.finalize(after)

    def unplug(self, device):
        self.next_step(f'user unplugs {device.name}')
        before = self.state
        after = self.h.unplug(before, before.unplug(device), device)

        self.print_info(before, after)
        self.check_g2(before, after, device)
        self.check_g4(after)
        self.finalize(after)

    def select(self, device):
        self.next_step(f'user selects {device.name}')
        before = self.state
        after = self.h.select(before, device)
        assert after.get_active() == device

        self.print_info(before, after)
        self.finalize(after)

    def expect(self, active_device):
        if (actual := self.state.get_active()) != active_device:
            print(
                f'violation: MANUAL: want {active_device.name} to be active but got {actual.name}'
            )

    def print_info(self, before: State, after: State):
        before_active = (
            ', '.join(dev.name for dev in before.active_devices()) or '(none)'
        )
        after_active = ', '.join(dev.name for dev in after.active_devices()) or '(none)'
        if before_active != after_active:
            print(f'active device: {before_active} => {after_active}')
        else:
            print('active device: no change')

        print('internal state:', self.h.internal_state())

        ordered_devices = [(dev, after.devices[dev]) for dev in self.devices]
        print(
            'status: [{}] {}'.format(
                ' '.join(
                    dev.name + ('*' if state.active else '')
                    for (dev, state) in ordered_devices
                    if state.connected
                ),
                ' '.join(
                    dev.name for (dev, state) in ordered_devices if not state.connected
                ),
            )
        )

    def check_g1(self, before: State, after: State, device: Device):
        if not before.num_active_devices():
            return
        if after.get_active() not in {device, before.get_active()}:
            print(
                f'violation: G1: Activated device {after.get_active().name} is not {before.get_active().name} or {device.name}'
            )

    def check_g2(self, before: State, after: State, device: Device):
        if not after.num_active_devices():
            return
        if before.devices[device].active:
            return
        if before.get_active() != after.get_active():
            print(
                f'violation: G2: Previously active device {before.get_active().name} was not unplugged but changed to {after.get_active().name}'
            )

    def check_g3(self, after: State, device: Device):
        if device.type != T.Headphone:
            return
        if after.get_active() != device:
            print(f'violation: G3: Headphone {device.name} plugged but not activated')

    def check_g4(self, after: State):
        # check G4
        try:
            g4_want = self.visited_states[after.connected_devices()]
        except KeyError:
            pass
        else:
            if g4_want != after.get_active():
                print(
                    'violation: G4: Last seen {} => {}; but selected {}'.format(
                        ''.join(dev.name for dev in after.connected_devices()),
                        g4_want,
                        after.get_active(),
                    )
                )

    def finalize(self, after):
        self.state = after
        self.visited_states[after.connected_devices()] = after.get_active()


class ChromiumUnitTestSimulator(Simulator):
    class Action(enum.Enum):
        PLUG = 'Plug'
        UNPLUG = 'Unplug'
        SELECT = 'Select'

    input_node_info = {
        T.Internal: ('INTERNAL_MIC', 'internal'),
        T.HDMI: None,
        T.USB: ('USB', 'usb'),
        T.Headphone: ('MIC', 'mic'),
    }
    output_node_info = {
        T.Internal: ('INTERNAL_SPEAKER', 'internal'),
        T.HDMI: ('HDMI', 'hdmi'),
        T.USB: ('USB', 'usb'),
        T.Headphone: ('HEADPHONE', 'headphone'),
    }

    def start(self):
        super().start()
        self.actions = []

    def plug(self, device):
        super().plug(device)
        self.actions.append((self.Action.PLUG, device, self.state, tuple(self.h.list)))

    def unplug(self, device):
        super().unplug(device)
        self.actions.append((self.Action.UNPLUG, device, self.state, tuple(self.h.list)))

    def select(self, device):
        if self.state.get_active() == device:
            # skip no-op select
            return
        super().select(device)
        self.actions.append((self.Action.SELECT, device, self.state, tuple(self.h.list)))

    def generate_testcase(self, file):
        name = ''.join(map(str.capitalize, re.split('/|_', self.name[6:-3])))
        self.generate_testcase_for_direction(name, 'input', self.input_node_info, file)
        self.generate_testcase_for_direction(name, 'output', self.output_node_info, file)

    def generate_testcase_for_direction(self, name, direction, node_info, file):
        print = functools.partial(builtins.print, file=file)

        device_type_counts = collections.Counter()
        nodes = {}
        setup = []
        for i, device in enumerate(self.devices, 1):
            if node_info[device.type] is None:
                return  # unsupported test
            test_type, var_prefix = node_info[device.type]
            var = nodes[device] = f'{var_prefix}{i}'
            setup.append(f'  AudioNode {var} = New{direction.capitalize()}Node("{test_type}");')

        print('TEST_F(AudioDeviceSelectionGeneratedTest, %s%s) {' % (name, direction.capitalize()))
        print('\n'.join(setup))
        for action, device, state, priorities in self.actions:
            selected = state.get_active()
            action_fn = {
                self.Action.PLUG: 'Plug',
                self.Action.UNPLUG: 'Unplug',
                self.Action.SELECT: 'Select',
            }[action]
            print()
            print('  %s(%s);' % (action_fn, nodes[device]))

            ordered_devices = [(dev, state.devices[dev]) for dev in self.devices]
            print(
                '  // Devices: [{}] {}'.format(
                    ' '.join(
                        nodes[dev] + ('*' if state.active else '')
                        for (dev, state) in ordered_devices
                        if state.connected
                    ),
                    ' '.join(
                        nodes[dev] for (dev, state) in ordered_devices if not state.connected
                    ),
                ).rstrip()
            )
            print('  // List: {}'.format(' < '.join(nodes[dev] for dev in priorities)))
            print('  EXPECT_EQ(Active%sNodeId(), %s.id);' % (direction.capitalize(), nodes[selected]))
        print('}')
        print()
