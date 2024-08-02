# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The invoker module is an abstraction over the commands module for running
command-line tools that cause specific side effects. An instance is used by
higher-level signing modules that pass and return structured inputs (e.g.
`model.CodeSignProduct`s).
"""


class Base(object):
    """Base is the parent class for all objects that act as invokers. An
    invoker's lifecycle is:
        1. Declare any command-line arguments that are provided by the
           invoker's operations, using `register_arguments()`.
        2. Construction with the parsed arguments and the config.

    The `invoker.Interface` instance is owned by an instance of
    `CodeSignConfig`.
    """

    @staticmethod
    def register_arguments(parser):
        """Registers Invoker-specific command line arguments.

        Args:
            parser: An `argparse.ArgumentParser` on which to register
                command-line arguments.
        """
        pass

    def __init__(self, args, config):
        """Creates a new invoker with the parsed command-line arguments.
        The passed `config` will own the instance of this invoker. This method
        can be used to verify that this invoker is compatible with both the
        specified `config` and the arguments passed to the constructor.

        Args:
            args: An `argparse.Namespace` after processing command-line
                arguments. See `Base.register_arguments`.
            config: The `config.CodeSignConfig`. Instances may store it
                in an instance variable.

        Raises:
            InvokerConfigError on an invalid argument or if the invoker
                is not compatible with the `config`.
        """
        pass


class InvokerConfigError(Exception):
    """An exception type used to report errors in configuring an invoker."""
    pass


class Interface(Base):
    """The `invoker.Interface` is the main interface for running high-level
    commands with side effects. It is composed of sub-invokers that have
    delegated responsibility for classes of operations.
    """

    class Signer(Base):
        """The Signer invoker is responsible for executing codesign-related
        operations.
        """

        def codesign(self, config, product, path):
            """Signs the specified `product` that is located inside the
            directory specified by `path`.

            Args:
                config: The `config.CodeSignConfig`.
                product: The `model.CodeSignedProduct` that controls signing
                    options.
                path: A string path at which `product` and any associated
                    resources can be found.
            """
            raise NotImplementedError('codesign')

    @property
    def signer(self):
        """Returns an instance of `invoker.Interface.Signer`."""
        raise NotImplementedError('signer')

    class Notarizer(Base):
        """The Notarizer invoker is responsible for executing
        notarization-related operations.
        """

        def submit(self, path, config):
            """Submits an artifact to Apple for notarization.

            Args:
                config: The `config.CodeSignConfig`.
                path: The path to the artifact that will be uploaded for
                    notarization.

            Returns:
                A string UUID from the notary service that represents the
                request.

            Raises:
                A `notarize.NotarizationError` on failure.
            """
            raise NotImplementedError('submit')

        def get_result(self, uuid, config):
            """Retrieves the current notarization status of the submission
            referenced by `uuid`.

            Args:
                config: The `config.CodeSignConfig`.
                uuid: The string UUID of the notarization submission.

            Returns:
                A `notarize.NotarizationResult` containing the status.

            Raises:
                A `notarize.NotarizationError` on failure.
            """
            raise NotImplementedError('get_result')

    @property
    def notarizer(self):
        """Returns an instance of `invoker.Interface.Notarizer`."""
        raise NotImplementedError('notarizer')
