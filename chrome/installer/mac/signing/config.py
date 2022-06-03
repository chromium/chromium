# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path

from .model import Distribution


class ConfigError(Exception):

    def __init__(self, attr_name):
        super(Exception, self).__init__(
            'Missing CodeSignConfig attribute "{}"'.format(attr_name))


class CodeSignConfig(object):
    """Code sign base configuration object.

    A CodeSignConfig contains properties that provide path component information
    to locate products for code signing and options that control the code
    signing process.

    There is a class hierarchy of CodeSignConfig objects, with the
    build_props_config.BuildPropsConfig containing injected variables from the
    build process. Configs for Chromium and Google Chrome subclass that to
    control signing options further. And then derived configurations are
    created for internal signing artifacts and when using |model.Distribution|
    objects.
    """

    def __init__(self,
                 identity,
                 installer_identity=None,
                 notary_user=None,
                 notary_password=None,
                 notary_asc_provider=None):
        """Creates a CodeSignConfig that will sign the product using the static
        properties on the class, using the code signing identity passed to the
        constructor.

        Args:
            identity: The name of the code signing identity to use for non-PKG
                files. This can be any value that `codesign -s <identity>`
                accepts, like the hex-encoded SHA1 hash of the certificate. Must
                not be None.
            installer_identity: The name of the code signing identity to use for
                PKG files. This will be passed as the parameter for the call to
                `productbuild --sign <identity>`. Note that a hex-encoded SHA1
                hash is not a valid option, as it is for |identity| above. The
                common name of the cert will work. If there is any distribution
                that is packaged in a PKG this must not be None.
            notary_user: Optional string username that will be used to
                authenticate to Apple's notary service if notarizing.
            notary_password: Optional string password or password reference
                (e.g. @keychain, see `xcrun altool -h`) that will be used to
                authenticate to Apple's notary service if notarizing.
            notary_asc_provider: Optional string that will be used as the
                `--asc-provider` argument to `xcrun altool`, to be used when
                notary_user is associated with multiple Apple developer teams.
        """
        assert identity
        self._identity = identity
        self._installer_identity = installer_identity
        self._notary_user = notary_user
        self._notary_password = notary_password
        self._notary_asc_provider = notary_asc_provider

    @staticmethod
    def is_chrome_branded():
        """Returns True if the build is an official Google Chrome build and
        should use Chrome-specific resources.

        This is a @staticmethod and not a @property so that it can be tested
        during the process of creating a CodeSignConfig object.
        """
        raise ConfigError('is_chrome_branded')

    @property
    def identity(self):
        """Returns the code signing identity that will be used to sign the
        products, everything but PKG files.
        """
        return self._identity

    @property
    def installer_identity(self):
        """Returns the code signing identity that will be used to sign the
        PKG file products.
        """
        return self._installer_identity

    @property
    def notary_user(self):
        """Returns the username for authenticating to Apple's notary service."""
        return self._notary_user

    @property
    def notary_password(self):
        """Returns the password or password reference for authenticating to
        Apple's notary service.
        """
        return self._notary_password

    @property
    def notary_asc_provider(self):
        """Returns the ASC provider for authenticating to Apple's notary service
        when notary_user is associatetd with multiple Apple developer teams.
        """
        return self._notary_asc_provider

    @property
    def app_product(self):
        """Returns the product name that is used for the outer .app bundle.
        This is displayed to the user in Finder.
        """
        raise ConfigError('app_product')

    @property
    def product(self):
        """Returns the branding product name. This can match |app_product|
        for some release channels. Other release channels may customize
        app_product, but components internal to the app bundle will still
        refer to |product|. This is used to locate the build products from
        the build system, while |app_product| is used when customizing for
        |model.Distribution| objects.
        """
        raise ConfigError('product')

    @property
    def version(self):
        """Returns the version of the application."""
        raise ConfigError('version')

    @property
    def base_bundle_id(self):
        """Returns the base CFBundleIdentifier that is used for the outer app
        bundle, and to which sub-component identifiers are appended.
        """
        raise ConfigError('base_bundle_id')

    @property
    def codesign_requirements_basic(self):
        """Returns the codesign --requirements string that is combined with
        a designated identifier requirement string of a
        |model.CodeSignedProduct|. This requirement is applied to all
        CodeSignedProducts.
        """
        return ''

    @property
    def codesign_requirements_outer_app(self):
        """Returns the codesign --requirements string for the outer app bundle.
        This is used in conjunction with |codesign_requirements_basic|."""
        return ''

    @property
    def provisioning_profile_basename(self):
        """Returns the basename of the provisioning profile used to sign the
        outer app bundle. This file with a .provisionprofile extension is
        located in the |packaging_dir|.
        """
        raise ConfigError('provisioning_profile_basename')

    @property
    def packaging_basename(self):
        """Returns the file basename of the packaged output files."""
        return '{}-{}'.format(self.app_product.replace(' ', ''), self.version)

    @property
    def distributions(self):
        """Returns a list of |model.Distribution| objects that customize the
        results of signing. This must contain at least one Distribution, which
        can be a default Distribution.
        """
        return [Distribution()]

    @property
    def run_spctl_assess(self):
        """Returns whether the final code signed binary should be assessed by
        Gatekeeper after signing.
        """
        return True

    # Computed Properties ######################################################

    @property
    def app_dir(self):
        """Returns the path to the outer app bundle directory."""
        return '{.app_product}.app'.format(self)

    @property
    def resources_dir(self):
        """Returns the path to the outer app's Resources directory."""
        return os.path.join(self.app_dir, 'Contents', 'Resources')

    @property
    def framework_dir(self):
        """Returns the path to the app's framework directory."""
        return '{0.app_dir}/Contents/Frameworks/{0.product} Framework.framework'.format(
            self)

    @property
    def packaging_dir(self):
        """Returns the path to the packaging and installer tools."""
        return '{.product} Packaging'.format(self)
