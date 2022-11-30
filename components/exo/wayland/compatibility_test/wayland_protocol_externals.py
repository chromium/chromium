# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from typing import Tuple

import wayland_protocol_data_classes

# Short aliases for typing
Protocol = wayland_protocol_data_classes.Protocol


def get_external_interfaces_for_protocol(
        protocol: Protocol) -> Tuple[str, ...]:
    """Gets the names of interfaces referenced but not defined by a protocol."""

    # Get the set of interfaces defined by the protocol
    defined_interfaces = set(i.name for i in protocol.interfaces)

    # Get and store the set of interfaces that are external to the protocol
    external_interface_names = set()
    for i in protocol.interfaces:
        external_interface_names.update(
            a.interface for r in i.requests for a in r.args
            if a.interface and a.interface not in defined_interfaces)
        external_interface_names.update(
            a.interface for e in i.events for a in e.args
            if a.interface and a.interface not in defined_interfaces)

    return tuple(external_interface_names)
