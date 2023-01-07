# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import wayland_protocol_data_classes

# Short aliases for typing
Message = wayland_protocol_data_classes.Message
MessageArg = wayland_protocol_data_classes.MessageArg
MessageArgType = wayland_protocol_data_classes.MessageArgType


def get_c_argument_type(arg_type: MessageArgType) -> str:
    """Maps a protocol argument type to a C type."""
    # Note: This should match what the core scanner.c does
    return {
        MessageArgType.INT.value: 'int32_t',
        MessageArgType.UINT.value: 'uint32_t',
        MessageArgType.FIXED.value: 'wl_fixed_t',
        MessageArgType.STRING.value: 'const char*',
        MessageArgType.FD.value: 'int32_t',
        MessageArgType.ARRAY.value: 'struct wl_array *',
        MessageArgType.NEW_ID.value: 'uint32_t',
    }[arg_type]


def get_c_arg_for_server_request_arg(arg: MessageArg) -> str:
    """Maps a protocol argument to C argument(s) for server-side requests."""
    # Note: This should match what the core scanner.c does
    if arg.type == MessageArgType.NEW_ID.value and not arg.interface:
        return f', const char *interface, uint32_t version, uint32_t {arg.name}'
    elif arg.type == MessageArgType.NEW_ID.value:
        return f', uint32_t {arg.name}'
    elif arg.type == MessageArgType.OBJECT.value:
        return f', struct {arg.interface} *{arg.name}'
    else:
        return f', {get_c_argument_type(arg.type)} {arg.name}'


def get_c_arg_for_server_event_arg(arg: MessageArg) -> str:
    """Maps a protocol argument to C argument(s) for server-side events."""
    # Note: This should match what the core scanner.c does
    if arg.type == MessageArgType.NEW_ID.value:
        return f', struct wl_resource* {arg.name}'
    else:
        return f', {get_c_argument_type(arg.type)} {arg.name}'


def get_c_return_type_for_client_request(message: Message) -> str:
    """Determines the C return type for client-side requests."""
    # Note: This should match what the core scanner.c does
    for arg in message.args:
        if arg.type == MessageArgType.NEW_ID.value:
            if arg.interface is None:
                return 'void *'
            else:
                return f'struct {arg.interface}'
    return 'void'


def get_c_arg_for_client_request_arg(arg: MessageArg) -> str:
    """Maps a protocol argument to C argument(s) for client-side requeuests."""
    # Note: This should match what the core scanner.c does
    if arg.type == MessageArgType.NEW_ID.value and not arg.interface:
        return ', const struct wl_interface *interface, uint32_t version'
    elif arg.type == MessageArgType.NEW_ID.value:
        return ''
    else:
        return f', {get_c_argument_type(arg.type)} {arg.name}'


def get_c_arg_for_client_event_arg(arg: MessageArg) -> str:
    """Maps a protocol argument to C argument(s) for client-side events."""
    if arg.type == MessageArgType.OBJECT.value and arg.interface is None:
        return f', void * {arg.name}'
    elif arg.type == MessageArgType.NEW_ID.value:
        return f', struct {arg.interface} *{arg.name}'
    elif arg.type == MessageArgType.OBJECT.value:
        return f', struct {arg.interface} *{arg.name}'
    else:
        return f', {get_c_argument_type(arg.type)} {arg.name}'
