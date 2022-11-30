# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import functools
import itertools
import re
from typing import Dict, Iterable, Tuple

import wayland_protocol_data_classes

# Short aliases for typing
Interface = wayland_protocol_data_classes.Interface
Protocol = wayland_protocol_data_classes.Protocol


def split_for_words(x: str) -> Tuple[str, ...]:
    """Split underscore_seperated identifiers into a component words."""
    return tuple(w.lower() for w in x.split('_'))


def kebab_case(x: str) -> str:
    """Convert the input identifier to kebab-case"""
    return '-'.join(split_for_words(x))


def pascal_case(x: str) -> str:
    """Convert the input identifier to PascalCase"""
    return ''.join(w.title() for w in split_for_words(x))


@functools.lru_cache(maxsize=None)
def get_base_human_readable_name_map(
        protocols: Iterable[Protocol]) -> Dict[str, str]:
    """Determines a mapping of each interface name to an identifier name."""

    # Default to the full interface name as the identifier.
    interface_identifiers = dict((interface.name, interface.name)
                                 for protocol in protocols
                                 for interface in protocol.interfaces)

    # Reserve the full interface names as identifiers, so nothing is
    # ever shortened to another interface name.
    used = set(interface_identifiers.keys())

    def set_identifier_name_for_protocol(names: Iterable[str],
                                         interface: Interface) -> None:
        """Set the first unused name in |names| as the name for |interface|"""
        for name in names:
            if name not in used:
                used.add(name)
                interface_identifiers[interface.name] = name
                return

    def set_identifier_names_for_interface(
            interfaces: Iterable[Interface]) -> None:
        """Try and set a shorter name for each interface"""
        # For each interface in protocol, try and find a shorter name to use
        # when representing instances of that interface.
        for interface in interfaces:
            name = interface.name
            name_no_prefix = name.split('_', 1)[-1]
            name_no_suffix = re.sub('_v\d+$', '', name)
            name_no_prefix_or_suffix = re.sub(r'_v\d+$', '', name_no_prefix)

            set_identifier_name_for_protocol(
                [name_no_prefix_or_suffix, name_no_prefix, name_no_suffix],
                interface)

    def prioritize_interfaces(
            interfaces: Iterable[Interface]) -> Tuple[Interface, ...]:
        DEFAULT_STABLE_PRIORITY = 100
        DEFAULT_UNSTABLE_PRIORITY = 101
        UNSTABLE_PREFIX = 'z'
        prefix_priorities = dict(wl_=0,
                                 wp_=1,
                                 xdg_=2,
                                 zwl_=10,
                                 zwp_=11,
                                 zxdg_=12,
                                 aura_=200,
                                 cr_=201,
                                 zaura_=202,
                                 zcr_=203)

        interfaces_by_priority = collections.defaultdict(list)

        for protocol in protocols:
            for interface in protocol.interfaces:
                name = interface.name
                priority = DEFAULT_STABLE_PRIORITY if not name.startswith(
                    UNSTABLE_PREFIX) else DEFAULT_UNSTABLE_PRIORITY
                for prefix, prefix_priority in prefix_priorities.items():
                    if name.startswith(prefix):
                        priority = prefix_priority
                        break
                interfaces_by_priority[priority].append(interface)

        return tuple(
            itertools.chain(*tuple(interfaces_by_priority[p]
                                   for p in sorted(interfaces_by_priority))))

    set_identifier_names_for_interface(
        prioritize_interfaces(
            tuple(
                itertools.chain(*tuple(protocol.interfaces
                                       for protocol in protocols)))))
    return interface_identifiers
