# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dataclasses
import enum
import os.path
from typing import Any, Dict, Iterable, List, Mapping, Optional, Tuple
import xml.etree.ElementTree

# Short aliases for typing
Element = xml.etree.ElementTree.Element


@dataclasses.dataclass(frozen=True)
class Description:
    """Holds the data from a Wayland protocol <description> subtree."""

    # The value of the name summary of the <description>.
    summary: str

    # The text content of the <description>.
    description: str

    @staticmethod
    def parse_xml(e: Element) -> Optional['Description']:
        return Description(summary=e.get('summary'),
                           description=e.text.strip()
                           if e.text else '') if e is not None else None


class MessageArgType(enum.Enum):
    """The valid types of an <arg>."""
    INT = 'int'
    UINT = 'uint'
    FIXED = 'fixed'
    STRING = 'string'
    FD = 'fd'  # File descriptor
    ARRAY = 'array'  # untyped Array
    NEW_ID = 'new_id'  # Special type for constructing
    OBJECT = 'object'  # An interface


@dataclasses.dataclass(frozen=True)
class MessageArg:
    """Holds the data from a Wayland protocol <arg> subtree."""

    # The containing Message, for context.
    message: 'Message' = dataclasses.field(repr=False, hash=False)

    # The value of the name attribute of the <arg>.
    name: str

    # The value of the type attribute of the <arg>.
    type: MessageArgType

    # The value of the optional summary attribute of the <arg>.
    summary: Optional[str]

    # The value of the optional interface attribute of the <arg>.
    interface: Optional[str]

    # The value of the optional nullable attribute of the <arg>.
    nullable: Optional[bool]

    # The value of the optional enum attribute of the <arg>.
    enum: Optional[str]

    # The optional <description> child element of the <arg>.
    description: Optional[Description]

    @staticmethod
    def parse_xml(message: 'Message', e: Element) -> 'MessageArg':
        return MessageArg(message=message,
                          name=e.get('name'),
                          type=e.get('type'),
                          summary=e.get('summary'),
                          interface=e.get('interface'),
                          nullable=e.get('allow-null') == 'true'
                          if e.get('allow-null') else None,
                          enum=e.get('enum'),
                          description=Description.parse_xml(
                              e.find('description')))


class RequestType(enum.Enum):
    """The valid types of a <request> message."""
    DESTRUCTOR = 'destructor'


@dataclasses.dataclass(frozen=True)
class Message:
    """Holds the data from a Wayland protocol <request> OR <event> subtree."""

    # The containing Interface, for context.
    interface: 'Interface' = dataclasses.field(repr=False, hash=False)

    # If true, this message is an <event> in the containing interface.
    # Otherwise it is a <request>.
    is_event: bool

    # The value of the name attribute of the <request> or <event>.
    name: str

    # The value of the optional type attribute of the <request> Always empty
    # for <events>.
    request_type: Optional[RequestType]

    # The value of the optional since attribute of the <request> or <event>.
    since: Optional[int]

    # The optional <description> child element of the <request> or <event>.
    description: Optional[Description]

    # The child <arg> elements of the <request> or <event>
    args: Tuple[MessageArg, ...] = dataclasses.field(init=False)

    @staticmethod
    def parse_xml(interface: 'Interface', is_event: bool,
                  e: Element) -> 'Message':
        message = Message(
            interface=interface,
            is_event=is_event,
            name=e.get('name'),
            request_type=e.get('type'),
            since=int(e.get('since')) if e.get('since') else None,
            description=Description.parse_xml(e.find('description')))

        # Note: This is needed to finish up since the instance is frozen.
        object.__setattr__(
            message, 'args',
            tuple(MessageArg.parse_xml(message, c) for c in e.findall('arg')))

        return message


@dataclasses.dataclass(frozen=True)
class EnumEntry:
    """Holds the data from a Wayland protocol <entry> subtree."""

    # The containing Enum, for context.
    enum: 'Enum' = dataclasses.field(repr=False, hash=False)

    # The value of the name attribute of the <entry>.
    name: str

    # The value of the value attribute of the <entry>.
    value: int

    # The value of the optional summary attribute of the <entry>.
    summary: Optional[str]

    # The value of the optional since attribute of the <entry>.
    since: Optional[int]

    # The optional <description> child element of the <request> or <event>.
    description: Optional[Description]

    @staticmethod
    def parse_xml(enum: 'Enum', e: Element) -> 'EnumEntry':
        return EnumEntry(
            enum=enum,
            name=e.get('name'),
            value=int(e.get('value'), 0),
            summary=e.get('summary'),
            since=int(e.get('since'), 0) if e.get('since') else None,
            description=Description.parse_xml(e.find('description')))


@dataclasses.dataclass(frozen=True)
class Enum:
    """Holds the data from a Wayland protocol <enum> subtree."""

    # The containing Interface, for context.
    interface: 'Interface' = dataclasses.field(repr=False, hash=False)

    # The value of the name attribute of the <enum>.
    name: str

    # The value of the optional since attribute of the <enum>.
    since: Optional[int]

    # The value of the optional bitfield attribute of the <enum>.
    bitfield: Optional[bool]

    # The optional <description> child element of the <request> or <event>.
    description: Optional[Description]

    # The child <entry> elements for the <enum>.
    entries: Tuple[EnumEntry, ...] = dataclasses.field(init=False)

    @staticmethod
    def parse_xml(interface: 'Interface', e: Element) -> 'Enum':
        enum = Enum(interface=interface,
                    name=e.get('name'),
                    since=int(e.get('since'), 0) if e.get('since') else None,
                    bitfield=e.get('bitfield') == 'true'
                    if e.get('bitfield') else None,
                    description=Description.parse_xml(e.find('description')))

        # Note: This is needed to finish up since the instance is frozen.
        object.__setattr__(
            enum, 'entries',
            tuple(EnumEntry.parse_xml(enum, c) for c in e.findall('entry')))

        return enum


@dataclasses.dataclass(frozen=True)
class Interface:
    """Holds the data from a Wayland protocol <interface> subtree."""

    # The containing Protocol, for context.
    protocol: 'Protocol' = dataclasses.field(repr=False, hash=False)

    # The value of the name attribute of the <interface>.
    name: str

    # The value of the version attribute of the <interface>.
    version: int

    # The optional <description> child element of the <interface>.
    description: Optional[Description]

    # The child <request> elements for the <interface>.
    requests: Tuple[Message, ...] = dataclasses.field(init=False)

    # The child <event> elements for the <interface>.
    events: Tuple[Message, ...] = dataclasses.field(init=False)

    # The child <enum> elements for the <interface>.
    enums: Tuple[Enum, ...] = dataclasses.field(init=False)

    @staticmethod
    def parse_xml(protocol: 'Protocol', e: Element) -> 'Interface':
        interface = Interface(protocol=protocol,
                              name=e.get('name'),
                              version=int(e.get('version'), 0),
                              description=Description.parse_xml(
                                  e.find('description')))

        # Note: This is needed to finish up since the instance is frozen.
        object.__setattr__(
            interface, 'requests',
            tuple(
                Message.parse_xml(interface, False, c)
                for c in e.findall('request')))
        object.__setattr__(
            interface, 'events',
            tuple(
                Message.parse_xml(interface, True, c)
                for c in e.findall('event')))
        object.__setattr__(
            interface, 'enums',
            tuple(Enum.parse_xml(interface, c) for c in e.findall('enum')))

        return interface


@dataclasses.dataclass(frozen=True)
class Copyright:
    """Holds the data from a Wayland protocol <copyright> subtree."""

    # The text content of the <copyright>.
    text: str

    @staticmethod
    def parse_xml(e: Element) -> Optional['Copyright']:
        return Copyright(text=e.text.strip()) if e is not None else None


@dataclasses.dataclass(frozen=True)
class Protocol:
    """Holds the data from a Wayland protocol <protocol> subtree."""

    # The universe of known protocols this is part of.
    protocols: 'Protocols' = dataclasses.field(repr=False, hash=False)

    # The containing base filename (no path, no extension), for context.
    filename: str

    # The value of the name attribute of the <protocol>.
    name: str

    # The optional <copyright> child element of the <protocol>.
    copyright: Optional[Copyright]

    # The optional <description> child element of the <protocol>.
    description: Optional[Description]

    # The child <interface> elements for the <protocol>.
    interfaces: Tuple[Interface, ...] = dataclasses.field(init=False)

    @staticmethod
    def parse_xml(protocols: 'Protocols', filename: str,
                  e: Element) -> 'Protocol':
        protocol = Protocol(protocols,
                            filename=filename,
                            name=e.get('name'),
                            copyright=Copyright.parse_xml(e.find('copyright')),
                            description=Description.parse_xml(
                                e.find('description')))

        # Note: This is needed to finish up since the instance is frozen.
        object.__setattr__(
            protocol, 'interfaces',
            tuple(
                Interface.parse_xml(protocol, i)
                for i in e.findall('interface')))

        return protocol


@dataclasses.dataclass(frozen=True)
class Protocols:
    """Holds the data from multiple Wayland protocol files."""

    # The parsed protocol dataclasses
    protocols: Tuple[Protocol, ...] = dataclasses.field(init=False)

    @staticmethod
    def parse_xml_files(filenames: Iterable[str]) -> 'Protocols':
        protocols = Protocols()

        # Note: This is needed to finish up since the instance is frozen.
        object.__setattr__(
            protocols, 'protocols',
            tuple(
                Protocol.parse_xml(
                    protocols,
                    os.path.splitext(os.path.basename(filename))[0],
                    xml.etree.ElementTree.parse(filename).getroot())
                for filename in filenames))

        return protocols
