# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import dataclasses
import functools
import io
import itertools
import sys
from typing import Any, Dict, Iterable, List, Mapping, Optional, Tuple

import wayland_protocol_data_classes
import wayland_protocol_identifiers

# Short aliases for typing
Message = wayland_protocol_data_classes.Message
Interface = wayland_protocol_data_classes.Interface
Protocol = wayland_protocol_data_classes.Protocol
RequestType = wayland_protocol_data_classes.RequestType


@functools.lru_cache(maxsize=None)
def get_interface_for_name(protocols: Iterable[Protocol],
                           target_interface_name: str) -> Optional[Interface]:
    """Given a name string, gets the interface that has that name, or None."""
    for protocol in protocols:
        for interface in protocol.interfaces:
            if interface.name == target_interface_name:
                return interface
    return None


@functools.lru_cache(maxsize=None)
def get_constructor_for_interface(
        target_interface: Interface) -> Optional[Message]:
    """Gets the message to use to construct the target interface, or None."""

    # Note: We assume there is only one constructor for any interface, and
    # return the first found, but there could be a protocol that defines
    # more than one way.

    for interface in target_interface.protocol.interfaces:
        for request in interface.requests:
            for arg in request.args:
                if (arg.type == 'new_id'
                        and arg.interface == target_interface.name):
                    return request
        for event in interface.events:
            for arg in event.args:
                if (arg.type == 'new_id'
                        and arg.interface == target_interface.name):
                    return event
    return None


@dataclasses.dataclass(frozen=True)
class ConstructionStepCtor:
    """Message and related data for a ConstructionStep"""

    # The construction step for the interface needed for this step.
    interface_step: 'ConstructionStep'

    # The message on the interface used to perform the construction in this
    # step. Note that while this is normally a client request, it can
    # occasionally be a server event. One example of that is the wl_data_offer
    # in the core Wayland protocol.
    message: Message

    # For request constructors, gives the construction steps for any
    # additional objects that are needed for constructing the target.
    # There are entries only for the arguments that are objects. The rest
    # are None. For event constructors this will always be empty.
    object_args: Tuple[Optional['ConstructionStep'], ...]


@dataclasses.dataclass(frozen=True)
class ConstructionStep:
    """Represents a step in the construction path of a target interface."""

    # Wayland interface constructed by this step
    interface: Interface

    # A reasonable human-readable name that can be used to generate variable
    # and function names for this step. The names will be unique within a single
    # generated sequence of steps.
    instance_name: str

    # The details of how to construct this interface, based on other
    # construction steps. This will be None if the interface in this step
    # is a Wayland global interface.
    ctor: Optional[ConstructionStepCtor]

    # Set to the minimum version needed for this interface.
    minimum_version: int


@functools.lru_cache(maxsize=None)
def get_construction_steps(
        target_interface: Interface) -> Tuple[ConstructionStep, ...]:
    """Generates the ConstructionSteps to construct a target interface."""

    # For brevity later, get the list of protocols as a local
    protocols = target_interface.protocol.protocols.protocols

    # Helper map for constructing human readable instance names
    base_human_readable_name_map = (
        wayland_protocol_identifiers.get_base_human_readable_name_map(
            target_interface.protocol.protocols.protocols))

    # Globals that will be needed (not ordered)
    global_steps = {}

    # Non-global instances that will be needed (ordered)
    instance_steps = []

    # To help ensure unique instance names
    uniquifier = collections.Counter()

    def unique_instance_name(prefix: str, name: str) -> str:
        def dedupe_words(name: str) -> str:
            # Otherwise a generated name might be "parent_surface_surface"
            # for a wl_surface passed as a parent_surface argument.
            words = name.split("_")
            if len(words) == 1:
                return name
            words = [
                w for i, w in enumerate(words[:-1]) if w not in words[i + 1]
            ] + [words[-1]]
            return "_".join(words)

        name = prefix + base_human_readable_name_map.get(name, name)
        name = dedupe_words(name)
        suffix = str(uniquifier.get(name, ''))
        uniquifier[name] += 1
        return name + suffix + "_"

    def recursive_construction_steps(current_target: Interface, prefix: str,
                                     minimum_version: int):
        ctor_message = get_constructor_for_interface(current_target)
        ctor = None

        if ctor_message is not None:
            # If we have a message, we have to use another interface to
            # create the current target.
            ctor_interface = recursive_construction_steps(
                ctor_message.interface, prefix,
                max(minimum_version,
                    ctor_message.since if ctor_message.since else 1))
            ctor_object_args = []

            if not ctor_message.is_event:
                # We may also have to construct other interfaces as well to
                # pass as object arguments. Those interfaces can be part of
                # any protocol, though normally it is either in the same
                # protocol or the core Wayland protocol.
                for arg in ctor_message.args:
                    arg_step = None
                    if arg.type == 'object':
                        arg_interface = get_interface_for_name(
                            protocols, arg.interface)
                        arg_step = recursive_construction_steps(
                            arg_interface, f'{prefix}{arg.name}_', 1)
                    ctor_object_args.append(arg_step)

            ctor = ConstructionStepCtor(ctor_interface, ctor_message,
                                        tuple(ctor_object_args))

        # Construct the step
        step = ConstructionStep(interface=current_target,
                                instance_name=unique_instance_name(
                                    prefix if ctor is not None else '',
                                    current_target.name),
                                ctor=ctor,
                                minimum_version=minimum_version)

        if ctor is None:
            # For a global, interface, we only make/get one instance
            step = global_steps.setdefault(current_target.name, step)
        else:
            # Otherwise store each individual step
            instance_steps.append(step)

        return step

    recursive_construction_steps(target_interface, '', 1)

    return tuple([global_steps[name]
                  for name in sorted(global_steps)] + instance_steps)


@functools.lru_cache(maxsize=None)
def get_destructor(interface: Interface) -> Optional[Message]:
    """Gets the Message that acts as the interface destructor, if present."""
    for message in interface.requests:
        if message.request_type == RequestType.DESTRUCTOR.value:
            return message
    return None


def get_minimum_version_to_construct(target: Interface) -> int:
    """Gets the minimum version of the global needed to construct a target."""
    def recursive_minimum(interface: Interface, minimum_version: int) -> int:
        ctor_message = get_constructor_for_interface(interface)

        # If there is no explicit constructor for this target, it must be a
        # global
        if not ctor_message:
            # Return the global interface
            return minimum_version

        # If the constructor has a "since", it constrains the minimum version
        if ctor_message.since:
            minimum_version = max(minimum_version, ctor_message.since)

        return recursive_minimum(ctor_message.interface, minimum_version)

    # Until proven otherwise, assume the first version can create the target
    return recursive_minimum(target, 1)


def get_versions_to_test_for_event_delivery(
        interface: Interface) -> Tuple[int, ...]:
    # Get the minimum interface version
    min_version = get_minimum_version_to_construct(interface)

    # Include all versions where events are introduced
    versions = set(event.since for event in interface.events
                   if event.since and event.since > min_version)
    # Include all versions one less than where events are introduced
    versions = versions.union(event.since - 1 for event in interface.events
                              if event.since and event.since - 1 > min_version)
    # Include the minimum and maximum versions
    versions = versions.union((min_version, interface.version))

    return tuple(versions)


def is_global_interface(interface: Interface) -> bool:
    """Returns true if the interface is a global interface."""
    return get_constructor_for_interface(interface) is None
