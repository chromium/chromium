# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import xml.etree.ElementTree

import wayland_protocol_utils

EnumValue = wayland_protocol_utils.EnumValue


class Description(object):
    """Holds the data from a Wayland protocol <description> subtree."""
    def __init__(self, summary, description):
        # type: (str, str) -> None

        # The value of the name summary of the <description>.
        object.__setattr__(self, 'summary', summary)

        # The text content of the <description>.
        object.__setattr__(self, 'description', description)

    def __setattr__(self, name, value):
        raise AttributeError('Frozen instance')

    def __repr__(self):
        return 'Description(summary=%r, description=%r)' % (self.summary,
                                                            self.description)

    def __hash__(self):
        return hash((self.summary, self.description))

    def __eq__(self, other):
        return ((self.summary, self.description) == (other.summary,
                                                     other.description))

    @staticmethod
    def parse_xml(e):
        # type: (Element) -> Optional[Description]

        return Description(summary=e.get('summary'),
                           description=e.text.strip()
                           if e.text else '') if e is not None else None


class MessageArgType(object):
    """The valid types of an <arg>."""
    INT = EnumValue('int')
    UINT = EnumValue('uint')
    FIXED = EnumValue('fixed')
    STRING = EnumValue('string')
    FD = EnumValue('fd')  # File descriptor
    ARRAY = EnumValue('array')  # untyped Array
    NEW_ID = EnumValue('new_id')  # Special type for constructing
    OBJECT = EnumValue('object')  # An interface


class MessageArg(object):
    """Holds the data from a Wayland protocol <arg> subtree."""
    def __init__(self, message, name, type, summary, interface, nullable, enum,
                 description):
        # type: (Message, str, MessageArgType, Optional[str], Optional[str],
        #        Optional[bool], Optional[str], Optional[Description]) -> None

        # The containing Message, for context.
        object.__setattr__(self, 'message', message)

        # The value of the name attribute of the <arg>.
        object.__setattr__(self, 'name', name)

        # The value of the type attribute of the <arg>.
        object.__setattr__(self, 'type', type)

        # The value of the optional summary attribute of the <arg>.
        object.__setattr__(self, 'summary', summary)

        # The value of the optional interface attribute of the <arg>.
        object.__setattr__(self, 'interface', interface)

        # The value of the optional nullable attribute of the <arg>.
        object.__setattr__(self, 'nullable', nullable)

        # The value of the optional enum attribute of the <arg>.
        object.__setattr__(self, 'enum', enum)

        # The optional <description> child element of the <arg>.
        object.__setattr__(self, 'description', description)

    def __setattr__(self, name, value):
        raise AttributeError('Frozen instance')

    def __repr__(self):
        return ('MessageArg(name=%r, type=%r, summary=%r, interface=%r, '
                'nullable=%r, enum=%r, description=%r)' %
                (self.name, self.type, self.summary, self.interface,
                 self.nullable, self.enum, self.description))

    def __hash__(self):
        return hash((self.name, self.type, self.summary, self.interface,
                     self.nullable, self.enum, self.description))

    def __eq__(self, other):
        return ((self.name, self.type, self.summary, self.interface,
                 self.nullable, self.enum,
                 self.description) == (other.name, other.type, other.summary,
                                       other.interface, other.nullable,
                                       other.enum, other.description))

    @staticmethod
    def parse_xml(message, e):
        # type: (Message, Element) -> MessageArg
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


class RequestType(object):
    """The valid types of a <request> message."""
    DESTRUCTOR = EnumValue('destructor')


class Message(object):
    """Holds the data from a Wayland protocol <request> OR <event> subtree."""
    def __init__(self, interface, is_event, name, request_type, since,
                 description):
        # type: (Interface, bool, str, Optional[RequestType], Optional[int],
        #        Optional[Description]) -> None

        # The containing Interface, for context.
        object.__setattr__(self, 'interface', interface)

        # If true, this message is an <event> in the containing interface.
        # Otherwise it is a <request>.
        object.__setattr__(self, 'is_event', is_event)

        # The value of the name attribute of the <request> or <event>.
        object.__setattr__(self, 'name', name)

        # The value of the optional type attribute of the <request> Always empty
        # for <events>.
        object.__setattr__(self, 'request_type', request_type)

        # The value of the optional since attribute of the <request> or <event>.
        object.__setattr__(self, 'since', since)

        # The optional <description> child element of the <request> or <event>.
        object.__setattr__(self, 'description', description)

        # The child <arg> elements of the <request> or <event>
        # type: Tuple[MessageArg, ...]
        object.__setattr__(self, 'args', ())

    def __setattr__(self, name, value):
        raise AttributeError('Frozen instance')

    def __repr__(self):
        return ('Message(is_event=%r, name=%r, request_type=%r, since=%r, '
                'description=%r, args=%r)' %
                (self.is_event, self.name, self.request_type, self.since,
                 self.description, self.args))

    def __hash__(self):
        return hash((self.is_event, self.name, self.request_type, self.since,
                     self.description, self.args))

    def __eq__(self, other):
        return ((self.is_event, self.name, self.request_type, self.since,
                 self.description,
                 self.args) == (other.is_event, other.name, other.request_type,
                                other.since, other.description, other.args))

    @staticmethod
    def parse_xml(interface, is_event, e):
        # type (Interface, bool, Element) -> Message

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


class EnumEntry(object):
    """Holds the data from a Wayland protocol <entry> subtree."""
    def __init__(self, enum, name, value, summary, since, description):
        # type: (Enum, str, int, Optional[str], Optional[int],
        #        Optional[Description]) -> None

        # The containing Enum, for context.
        object.__setattr__(self, 'enum', enum)

        # The value of the name attribute of the <entry>.
        object.__setattr__(self, 'name', name)

        # The value of the value attribute of the <entry>.
        object.__setattr__(self, 'value', value)

        # The value of the optional summary attribute of the <entry>.
        object.__setattr__(self, 'summary', summary)

        # The value of the optional since attribute of the <entry>.
        object.__setattr__(self, 'since', since)

        # The optional <description> child element of the <request> or <event>.
        object.__setattr__(self, 'description', description)

    def __setattr__(self, name, value):
        raise AttributeError('Frozen instance')

    def __repr__(self):
        return ('EnumEntry(name=%r, value=%r, summary=%r, since=%r,'
                'description=%r)' % (self.name, self.value, self.summary,
                                     self.since, self.description))

    def __hash__(self):
        return hash((self.name, self.value, self.summary, self.since,
                     self.description))

    def __eq__(self, other):
        return ((self.name, self.value, self.summary, self.since,
                 self.description) == (other.enum, other.name, other.value,
                                       other.summary, other.since,
                                       other.description))

    @staticmethod
    def parse_xml(enum, e):
        # type: (Enum, Element) -> EnumEntry:

        return EnumEntry(
            enum=enum,
            name=e.get('name'),
            value=int(e.get('value'), 0),
            summary=e.get('summary'),
            since=int(e.get('since'), 0) if e.get('since') else None,
            description=Description.parse_xml(e.find('description')))


class Enum(object):
    """Holds the data from a Wayland protocol <enum> subtree."""
    def __init__(self, interface, name, since, bitfield, description):
        # type: (Interface, str, Optional[int], Optional[bool],
        #        Optional[Description]) -> None

        # The containing Interface, for context.
        object.__setattr__(self, 'interface', interface)

        # The value of the name attribute of the <enum>.
        object.__setattr__(self, 'name', name)

        # The value of the optional since attribute of the <enum>.
        object.__setattr__(self, 'since', since)

        # The value of the optional bitfield attribute of the <enum>.
        object.__setattr__(self, 'bitfield', bitfield)

        # The optional <description> child element of the <request> or <event>.
        object.__setattr__(self, 'description', description)

        # The child <entry> elements for the <enum>.
        # type: Tuple[EnumEntry, ...]
        object.__setattr__(self, 'entries', ())

    def __setattr__(self, name, value):
        raise AttributeError('Frozen instance')

    def __repr__(self):
        return ('EnumEntry(name=%r, since=%r, bitfield=%r, description=%r, '
                'entries=%r)' % (self.name, self.since, self.bitfield,
                                 self.description, self.entries))

    def __hash__(self):
        return hash((self.name, self.since, self.bitfield, self.description,
                     self.entries))

    def __eq__(self, other):
        return ((self.name, self.since, self.bitfield, self.description,
                 self.entries) == (other.name, other.since, other.bitfield,
                                   other.description, other.entries))

    @staticmethod
    def parse_xml(interface, e):
        # type: (Interface, Element) -> Enum

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


class Interface(object):
    """Holds the data from a Wayland protocol <interface> subtree."""
    def __init__(self, protocol, name, version, description):
        # type: (Protocol, str, int, Optional[Description]) -> None

        # The containing Protocol, for context.
        object.__setattr__(self, 'protocol', protocol)

        # The value of the name attribute of the <interface>.
        object.__setattr__(self, 'name', name)

        # The value of the version attribute of the <interface>.
        object.__setattr__(self, 'version', version)

        # The optional <description> child element of the <interface>.
        object.__setattr__(self, 'description', description)

        # The child <request> elements for the <interface>.
        # type: Tuple[Message, ...]
        object.__setattr__(self, 'requests', ())

        # The child <event> elements for the <interface>.
        # type: Tuple[Message, ...]
        object.__setattr__(self, 'events', ())

        # The child <enum> elements for the <interface>.
        # type: Tuple[Enum, ...]
        object.__setattr__(self, 'enums', ())

    def __setattr__(self, name, value):
        raise AttributeError('Frozen instance')

    def __repr__(self):
        return ('Interface(name=%r, version=%r, description=%r, requests=%r, '
                'events=%r, enums=%r)' %
                (self.name, self.version, self.description, self.requests,
                 self.events, self.enums))

    def __hash__(self):
        return hash((self.name, self.version, self.description, self.requests,
                     self.events, self.enums))

    def __eq__(self, other):
        return ((self.name, self.version, self.description, self.requests,
                 self.events,
                 self.enums) == (other.name, other.version, other.description,
                                 other.requests, other.events.other, enums))

    @staticmethod
    def parse_xml(protocol, e):
        # type: (Protocol, Element) -> Interface
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


class Copyright(object):
    """Holds the data from a Wayland protocol <copyright> subtree."""
    def __init__(self, text):
        # type: (str) -> None

        # The text content of the <copyright>.
        object.__setattr__(self, 'text', text)

    def __setattr__(self, name, value):
        raise AttributeError('Frozen instance')

    def __repr__(self):
        return 'Copyright(text=%r)' % (self.text, )

    def __hash__(self):
        return hash((self.text, ))

    def __eq__(self, other):
        return (self.text, ) == (other.text, )

    @staticmethod
    def parse_xml(e):
        # type: (Element) -> Optional['Copyright']

        return Copyright(text=e.text.strip()) if e is not None else None


class Protocol(object):
    """Holds the data from a Wayland protocol <protocol> subtree."""
    def __init__(self, protocols, filename, name, copyright, description):
        # type: (Protocols, str, str, Optional[Copyright],
        #        Optional[Description]) -> None

        # The universe of known protocols this is part of.
        object.__setattr__(self, 'protocols', protocols)

        # The containing base filename (no path, no extension), for context.
        object.__setattr__(self, 'filename', filename)

        # The value of the name attribute of the <protocol>.
        object.__setattr__(self, 'name', name)

        # The optional <copyright> child element of the <protocol>.
        object.__setattr__(self, 'copyright', copyright)

        # The optional <description> child element of the <protocol>.
        object.__setattr__(self, 'description', description)

        # The child <interface> elements for the <protocol>.
        # type: Tuple[Interface, ...]
        object.__setattr__(self, 'interfaces', ())

    def __setattr__(self, name, value):
        raise AttributeError('Frozen instance')

    def __repr__(self):
        return ('Protocol(filename=%r, name=%r, copyright=%r, description=%r, '
                'interfaces=%r)' % (self.filename, self.name, self.copyright,
                                    self.description, self.interfaces))

    def __hash__(self):
        return hash((self.filename, self.name, self.copyright,
                     self.description, self.interfaces))

    def __eq__(self, other):
        return ((self.filename, self.name, self.copyright, self.description,
                 self.interfaces) == (other.filename, other.name,
                                      other.copyright, other.description,
                                      other.interfaces))

    @staticmethod
    def parse_xml(protocols, filename, e):
        # type: (Protocols, str, Element) -> Protocol

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


class Protocols(object):
    """Holds the data from multiple Wayland protocol files."""
    def __init__(self):
        # type: () -> None

        # The parsed protocol dataclasses
        # type: Tuple[Protocol, ...]
        object.__setattr__(self, 'protocols', ())

    def __setattr__(self, name, value):
        raise AttributeError('Frozen instance')

    def __repr__(self):
        return 'Protocols(protocols=%r)' % (self.protocols, )

    def __hash__(self):
        return hash((self.protocols, ))

    def __eq__(self, other):
        return ((self.protocols, ) == (other.protocols, ))

    @staticmethod
    def parse_xml_files(filenames):
        # type: (Iterable[str]) -> Protocols

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
