# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path

from signing.model import Distribution, NotarizeAndStapleLevel


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
                 invoker=None,
                 identity=None,
                 installer_identity=None,
                 codesign_requirements_basic='',
                 notarize=NotarizeAndStapleLevel.STAPLE):
        """Creates a CodeSignConfig that will sign the product using the static
        properties on the class, using the code signing identity passed to the
        constructor.

        Args:
            invoker: The operation invoker. This may be either an instantaited
                |invoker.Interface| or a 1-arg callable that takes this instance
                of |CodeSignConfig| and returns an instance of
                |invoker.Interface|.
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
            codesign_requirements_basic: Optional string to specify the default
                basic `codesign --requirements`.
            notarize: The |model.NotarizeAndStapleLevel|.
        """
        assert identity is not None
        assert type(identity) is str
        assert invoker is not None
        self._identity = identity
        self._installer_identity = installer_identity
        self._codesign_requirements_basic = codesign_requirements_basic
        self._notarize = notarize
        if callable(invoker):
            # Create a placeholder for the invoker in case the initializer
            # accesses the field on the config.
            self._invoker = None
            invoker = invoker(self)
        self._invoker = invoker

    @staticmethod
    def is_chrome_branded():
        """Returns True if the build is an official Google Chrome build and
        should use Chrome-specific resources.

        This is a @staticmethod and not a @property so that it can be tested
        during the process of creating a CodeSignConfig object.
        """
        raise ConfigError('is_chrome_branded')

    @property
    def invoker(self):
        """Returns the |invoker.Interface| instance for signing and notarizing.
        """
        return self._invoker

    @property
    def enable_updater(self):
        """Returns True if the build should use updater-related resources.
        """
        raise ConfigError('enable_updater')

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
    def notarize(self):
        """Returns the |model.NotarizeAndStapleLevel| that controls how, if
        at all, notarization and stapling of CodeSignedProducts should occur.
        """
        return self._notarize

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
        return self._codesign_requirements_basic

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

    @property
    def inject_get_task_allow_entitlement(self):
        """Returns whether the com.apple.security.get-task-allow entitlement
        should be added to all entitlement files. This will permit attaching a
        debugger to a signed process, if the binary was signed with the
        hardened runtime.
        """
        return False

    @property
    def main_executable_pinned_geometry(self):
        """An optional tuple of pinned architecture offset pairs. If set the
        pinned offsets will be compared with the apps signed main executable
        offsets. If they do not match an exception will be thrown. Offsets are
        compared in the order they are provided.
        Provide the tuple in the following format:
        (('x86_64', 16384), ('arm64', 294912))
        Provide the tuple of pinned offsets in bytes and in the desired order.
        For non-universal binaries this format can be used: (('arm64', 0),)
        """
        return None

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
