# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Signing Model Objects

This module contains classes that encapsulate data about the signing process.
"""

import os.path


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
            options: Options flags to pass to `codesign --options`, from
                |CodeSignOptions|.
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
            verify_options: Flags to pass to `codesign --verify`, from
                |VerifyOptions|.
        """
        self.path = path
        self.identifier = identifier
        if not CodeSignOptions.valid(options):
            raise ValueError('Invalid CodeSignOptions: {}'.format(options))
        self.options = options
        self.requirements = requirements
        self.identifier_requirement = identifier_requirement
        self.sign_with_identifier = sign_with_identifier
        self.entitlements = entitlements
        if not VerifyOptions.valid(verify_options):
            raise ValueError('Invalid VerifyOptions: {}'.format(verify_options))
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
        if config.identity == '-':
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


def make_enum(class_name, options):
    """Makes a new class type for an enum.

    Args:
        class_name: Name of the new type to make.
        options: A dictionary of enum options to use. The keys will become
            attributes on the class, and the values will be wrapped in a tuple
            so that the options can be joined together.

    Returns:
        A new class for the enum.
    """
    attrs = {}

    @classmethod
    def valid(cls, opts_to_check):
        """Tests if the specified |opts_to_check| are valid.

        Args:
            options: Iterable of option strings.

        Returns:
            True if all the options are valid, False if otherwise.
        """
        if opts_to_check is None:
            return True
        valid_values = options.values()
        return all([option in valid_values for option in opts_to_check])

    attrs['valid'] = valid

    for name, value in options.items():
        assert type(name) is str
        assert type(value) is str
        attrs[name] = (value,)

    return type(class_name, (object,), attrs)


"""Enum for the options that can be specified when validating the results of
code signing.

These options are passed to `codesign --verify` after the
|CodeSignedProduct| has been signed.
"""
VerifyOptions = make_enum(
    'signing.model.VerifyOptions', {
        'DEEP': '--deep',
        'NO_STRICT': '--no-strict',
        'IGNORE_RESOURCES': '--ignore-resources',
    })

CodeSignOptions = make_enum(
    'signing.model.CodeSignOptions', {
        'RESTRICT': 'restrict',
        'LIBRARY_VALIDATION': 'library',
        'HARDENED_RUNTIME': 'runtime',
        'KILL': 'kill',
    })


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
                 package_as_pkg=False):
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
        """
        self.channel = channel
        self.branding_code = branding_code
        self.app_name_fragment = app_name_fragment
        self.packaging_name_fragment = packaging_name_fragment
        self.product_dirname = product_dirname
        self.creator_code = creator_code
        self.channel_customize = channel_customize
        self.package_as_dmg = package_as_dmg
        self.package_as_pkg = package_as_pkg

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
                profile = base_config.provisioning_profile_basename
                if profile and this.channel_customize:
                    return '{}_{}'.format(profile, this.app_name_fragment)
                return profile

            @property
            def packaging_basename(self):
                if this.packaging_name_fragment:
                    return '{}-{}-{}'.format(
                        self.app_product.replace(' ', ''), self.version,
                        this.packaging_name_fragment)
                return super(DistributionCodeSignConfig,
                             self).packaging_basename

        return DistributionCodeSignConfig(
            base_config.identity, base_config.installer_identity,
            base_config.notary_user, base_config.notary_password,
            base_config.notary_asc_provider)


class Paths(object):
    """Paths holds the three file path contexts for signing operations.

    The input directory always remains un-modified.
    The output directory is where final, signed products are stored.
    The work directory is set by internal operations.
    """

    def __init__(self, input, output, work):
        self._input = input
        self._output = output
        self._work = work

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
