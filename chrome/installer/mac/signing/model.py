# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Signing Model Objects

This module contains classes that encapsulate data about the signing process.
"""

import enum
import os.path
import re
import string

from signing import commands


def _get_identity_hash(identity):
    """Returns a string of the SHA-1 hash of a specified keychain identity.

    Args:
        identity: A string specifying the identity.

    Returns:
        A string with the hash, with a-f in lower case.

    Raises:
        ValueError: If the identity cannot be found.
    """
    if len(identity) == 40 and all(ch in string.hexdigits for ch in identity):
        return identity.lower()

    command = ['security', 'find-certificate', '-a', '-c', identity, '-Z']
    output = commands.run_command_output(command)

    hash_match = re.search(
        b'^SHA-1 hash: ([0-9A-Fa-f]{40})$', output, flags=re.MULTILINE)
    if not hash_match:
        raise ValueError('Cannot find identity', identity)

    return hash_match.group(1).decode('utf-8').lower()


class CodeSignedProduct(object):
    """Represents a build product that will be signed with `codesign(1)`."""

    def __init__(self,
                 path,
                 identifier,
                 options=None,
                 requirements=None,
                 identifier_requirement=True,
                 sign_with_identifier=False,
                 entitlements=None,
                 verify_options=None):
        """A build product to be codesigned.

        Args:
            path: The path to the product to be signed. This is relative to a
                work directory containing the build products.
            identifier: The unique identifier set when code signing. This is
                only explicitly passed with the `--identifier` flag if
                |sign_with_identifier| is True.
            options: |CodeSignOptions| flags to pass to `codesign --options`.
            requirements: String for additional `--requirements` to pass to the
                `codesign` command. These are joined with a space to the
                |config.CodeSignConfig.codesign_requirements_basic| string. See
                |CodeSignedProduct.requirements_string()| for details.
            identifier_requirement: If True, a designated identifier requirement
                based on |identifier| will be inserted into the requirements
                string. If False, then no designated requirement will be
                generated based on the identifier.
            sign_with_identifier: If True, then the identifier will be specified
                when running the `codesign` command. If False, `codesign` will
                infer the identifier itself.
            entitlements: File name of the entitlements file to sign the product
                with. The file should reside in the |Paths.packaging_dir|.
            verify_options: |VerifyOptions| flags to pass to `codesign
                --verify`.
        """
        self.path = path
        self.identifier = identifier
        if options and not isinstance(options, CodeSignOptions):
            raise ValueError('Must be a CodeSignOptions')
        self.options = options
        self.requirements = requirements
        self.identifier_requirement = identifier_requirement
        self.sign_with_identifier = sign_with_identifier
        self.entitlements = entitlements
        if verify_options and not isinstance(verify_options, VerifyOptions):
            raise ValueError('Must be a VerifyOptions')
        self.verify_options = verify_options

    def requirements_string(self, config):
        """Produces a full requirements string for the product.

        Args:
            config: A |config.CodeSignConfig| object.

        Returns:
            A string for designated requirements of the product, which can be
            passed to `codesign --requirements`.
        """
        # If the signing identity indicates ad-hoc (i.e. no real signing
        # identity), do not enforce any requirements. Ad hoc signing will append
        # a hash to the identifier, which would violate the
        # identifier_requirement and most other requirements that would be
        # specified.
        #
        # Similarly, if no explicit requirements are available, let codesign
        # --sign use its defaults, which should be appropriate in any case where
        # requirement customization is unnecessary.
        if config.identity == '-' or (not self.requirements and
                                      not config.codesign_requirements_basic):
            return ''

        reqs = []
        if self.identifier_requirement:
            reqs.append('designated => identifier "{identifier}"'.format(
                identifier=self.identifier))
        if self.requirements:
            reqs.append(self.requirements)
        if config.codesign_requirements_basic:
            reqs.append(config.codesign_requirements_basic)
        return ' '.join(reqs)

    def __repr__(self):
        return 'CodeSignedProduct(identifier={0.identifier}, ' \
                'options={0.options}, path={0.path})'.format(self)


class VerifyOptions(enum.Flag):
    """Enum for the options that can be specified when validating the results of
    code signing.

    These options are passed to `codesign --verify` after the
    |CodeSignedProduct| has been signed.
    """
    DEEP = enum.auto()
    STRICT = enum.auto()
    NO_STRICT = enum.auto()
    IGNORE_RESOURCES = enum.auto()

    def to_list(self):
        result = []
        values = {
            self.DEEP: '--deep',
            self.STRICT: '--strict',
            self.NO_STRICT: '--no-strict',
            self.IGNORE_RESOURCES: '--ignore-resources',
        }

        for key, value in values.items():
            if key & self:
                result.append(value)

        return sorted(result)


class CodeSignOptions(enum.Flag):
    """Enum for the options that can be specified when signing the code.

    These options are passed to `codesign --sign --options`.
    """
    RESTRICT = enum.auto()
    LIBRARY_VALIDATION = enum.auto()
    HARDENED_RUNTIME = enum.auto()
    KILL = enum.auto()
    # Specify the components of HARDENED_RUNTIME that are also available on
    # older macOS versions.
    FULL_HARDENED_RUNTIME_OPTIONS = (
        RESTRICT | LIBRARY_VALIDATION | HARDENED_RUNTIME | KILL)

    def to_comma_delimited_string(self):
        result = []
        values = {
            self.RESTRICT: 'restrict',
            self.LIBRARY_VALIDATION: 'library',
            self.HARDENED_RUNTIME: 'runtime',
            self.KILL: 'kill',
        }

        for key, value in values.items():
            if key & self:
                result.append(value)

        return ','.join(sorted(result))


class NotarizeAndStapleLevel(enum.Enum):
    """An enum specifying the level of notarization and stapling to do.

    `NONE` means no notarization tasks should be performed.

    `NOWAIT` means to submit the signed application and packaging to Apple for
    notarization, but not to wait for a reply.

    `WAIT_NOSTAPLE` means to submit the signed application and packaging to
    Apple for notarization, and wait for a reply, but not to staple the
    resulting notarization ticket.

    `STAPLE` means to submit the signed application and packaging to Apple for
    notarization, wait for a reply, and staple the resulting notarization
    ticket.
    """
    NONE = 0
    NOWAIT = 1
    WAIT_NOSTAPLE = 2
    STAPLE = 3

    def should_notarize(self):
        return self.value > self.NONE.value

    def should_wait(self):
        return self.value > self.NOWAIT.value

    def should_staple(self):
        return self.value > self.WAIT_NOSTAPLE.value

    def __str__(self):
        return self.name.lower().replace('_', '-')

    @classmethod
    def from_string(cls, str):
        try:
            return cls[str.upper().replace('-', '_')]
        except KeyError:
            raise ValueError(f'Invalid NotarizeAndStapleLevel: {str}')


class Distribution(object):
    """A Distribution represents a final, signed, and potentially channel-
    customized Chrome product.

    Channel customization refers to modifying parts of the app bundle structure
    to have different file names, internal identifiers, and assets.
    """

    def __init__(self,
                 channel=None,
                 branding_code=None,
                 app_name_fragment=None,
                 packaging_name_fragment=None,
                 product_dirname=None,
                 creator_code=None,
                 channel_customize=False,
                 package_as_dmg=True,
                 package_as_pkg=False,
                 package_as_zip=False,
                 inflation_kilobytes=0):
        """Creates a new Distribution object. All arguments are optional.

        Args:
            channel: The release channel for the product.
            branding_code: A branding code helps track how users acquired the
                product from various marketing channels.
            app_name_fragment: If present, this string fragment is appended to
                the |config.CodeSignConfig.app_product|. This renames the binary
                and outer app bundle.
            packaging_name_fragment: If present, this is appended to the
                |config.CodeSignConfig.packaging_basename| to help differentiate
                different |branding_code|s.
            product_dirname: If present, this string value is set in the app's
                Info.plist with the key "CrProductDirName". This key influences
                the browser's default user-data-dir location.
            creator_code: If present, this will set a new macOS creator code
                in the Info.plist "CFBundleSignature" key and in the PkgInfo
                file. If this is not specified, the original values from the
                build products will be kept.
            channel_customize: If True, then the product will be modified in
                several ways:
                - The |channel| will be appended to the
                  |config.CodeSignConfig.base_bundle_id|.
                - The product will be renamed with |app_name_fragment|.
                - Different assets will be used for icons in the app.
            package_as_dmg: If True, then a .dmg file will be created containing
                the product.
            package_as_pkg: If True, then a .pkg file will be created containing
                the product.
            package_as_zip: If True, then a .zip file will be created containing
                the product.
            inflation_kilobytes: If non-zero, a blob of this size will be
                inserted into the DMG. Incompatible with package_as_pkg = True.
        """
        if channel_customize:
            # Side-by-side channels must have a distinct names and creator
            # codes, as well as keep their user data in separate locations.
            assert channel
            assert app_name_fragment
            assert product_dirname
            assert creator_code

        self.channel = channel
        self.branding_code = branding_code
        self.app_name_fragment = app_name_fragment
        self.packaging_name_fragment = packaging_name_fragment
        self.product_dirname = product_dirname
        self.creator_code = creator_code
        self.channel_customize = channel_customize
        self.package_as_zip = package_as_zip
        self.package_as_dmg = package_as_dmg
        self.package_as_pkg = package_as_pkg
        self.inflation_kilobytes = inflation_kilobytes

        # inflation_kilobytes are only inserted into DMGs
        assert not self.inflation_kilobytes or self.package_as_dmg

    def brandless_copy(self):
        """Derives and returns a copy of this Distribution object, identical
        except for not having a branding code.

        This is useful in the case where a non-branded app bundle needs to be
        created with otherwise the same configuration.
        """
        return Distribution(self.channel, None, self.app_name_fragment,
                            self.packaging_name_fragment, self.product_dirname,
                            self.creator_code, self.channel_customize,
                            self.package_as_dmg, self.package_as_pkg,
                            self.package_as_zip)

    def to_config(self, base_config):
        """Produces a derived |config.CodeSignConfig| for the Distribution.

        Args:
            base_config: The base CodeSignConfig to derive.

        Returns:
            A new CodeSignConfig instance that uses information in the
            Distribution to alter various properties of the |base_config|.
        """
        this = self

        class DistributionCodeSignConfig(base_config.__class__):

            @property
            def base_config(self):
                return base_config

            @property
            def distribution(self):
                return this

            @property
            def app_product(self):
                if this.channel_customize:
                    return '{} {}'.format(base_config.app_product,
                                          this.app_name_fragment)
                return base_config.app_product

            @property
            def base_bundle_id(self):
                base_bundle_id = base_config.base_bundle_id
                if this.channel_customize:
                    return base_bundle_id + '.' + this.channel
                return base_bundle_id

            @property
            def provisioning_profile_basename(self):
                profile_basename = base_config.provisioning_profile_basename
                if not profile_basename:
                    return profile_basename

                if this.channel_customize:
                    profile_basename = '{}_{}'.format(profile_basename,
                                                      this.app_name_fragment)
                if base_config.identity:
                    profile_basename = '{}.{}'.format(
                        profile_basename,
                        _get_identity_hash(base_config.identity))

                return profile_basename

            @property
            def packaging_basename(self):
                if this.packaging_name_fragment:
                    return '{}-{}-{}'.format(
                        self.app_product.replace(' ', ''), self.version,
                        this.packaging_name_fragment)
                return super(DistributionCodeSignConfig,
                             self).packaging_basename

        return DistributionCodeSignConfig(
            **pick(base_config, ('invoker', 'identity', 'installer_identity',
                                 'codesign_requirements_basic')))


class Paths(object):
    """Paths holds the three file path contexts for signing operations.

    The input directory always remains un-modified.
    The output directory is where final, signed products are stored.
    The work directory is set by internal operations.
    """

    def __init__(self, input, output, work):
        self._input = os.path.abspath(input)
        self._output = os.path.abspath(output)
        self._work = work
        if self._work:
            self._work = os.path.abspath(self._work)

    @property
    def input(self):
        return self._input

    @property
    def output(self):
        return self._output

    @property
    def work(self):
        return self._work

    def packaging_dir(self, config):
        """Returns the path to the product packaging directory, which contains
        scripts and assets used in signing.

        Args:
            config: The |config.CodeSignConfig| object.

        Returns:
            Path to the packaging directory.
        """
        return os.path.join(self.input, '{} Packaging'.format(config.product))

    def replace_work(self, new_work):
        """Creates a new Paths with the same input and output directories, but
        with |work| set to |new_work|."""
        return Paths(self.input, self.output, new_work)

    def __eq__(self, other):
        if not isinstance(other, self.__class__):
            return False
        return (self._input == other._input and
                self._output == other._output and self._work == other._work)

    def __repr__(self):
        return 'Paths(input={0.input}, output={0.output}, ' \
                'work={0.work})'.format(self)


def pick(o, keys):
    """Returns a dictionary with the values of |o| from the keys specified
    in |keys|.

    Args:
        o: object or dictionary, An object to take values from.
        keys: list of string, Keys to pick from |o|.

    Returns:
        A new dictionary with keys from |keys| and values from |o|. Keys not
        in |o| will be omitted.
    """
    d = {}
    iterable = hasattr(o, '__getitem__')
    for k in keys:
        if hasattr(o, k):
            d[k] = getattr(o, k)
        elif iterable and k in o:
            d[k] = o[k]
    return d
